package builder

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"runtime"
	"strings"
	"sync"

	"github.com/docker/docker/api"
	"github.com/docker/docker/builder/parser"
	"github.com/docker/docker/cliconfig"
	"github.com/docker/docker/daemon"
	"github.com/docker/docker/graph/tags"
	"github.com/docker/docker/pkg/archive"
	"github.com/docker/docker/pkg/httputils"
	"github.com/docker/docker/pkg/parsers"
	"github.com/docker/docker/pkg/progressreader"
	"github.com/docker/docker/pkg/streamformatter"
	"github.com/docker/docker/pkg/stringid"
	"github.com/docker/docker/pkg/ulimit"
	"github.com/docker/docker/pkg/urlutil"
	"github.com/docker/docker/registry"
	"github.com/docker/docker/runconfig"
	"github.com/docker/docker/utils"
)

// When downloading remote contexts, limit the amount (in bytes)
// to be read from the response body in order to detect its Content-Type
const maxPreambleLength = 100

// whitelist of commands allowed for a commit/import
var validCommitCommands = map[string]bool{
	"cmd":        true,
	"entrypoint": true,
	"env":        true,
	"expose":     true,
	"label":      true,
	"onbuild":    true,
	"user":       true,
	"volume":     true,
	"workdir":    true,
}

// Config contains all configs for a build job
type Config struct {
	DockerfileName string
	RemoteURL      string
	RepoName       string
	SuppressOutput bool
	NoCache        bool
	Remove         bool
	ForceRemove    bool
	Pull           bool
	Memory         int64
	MemorySwap     int64
	CPUShares      int64
	CPUPeriod      int64
	CPUQuota       int64
	CPUSetCpus     string
	CPUSetMems     string
	CgroupParent   string
	Ulimits        []*ulimit.Ulimit
	AuthConfigs    map[string]cliconfig.AuthConfig

	Stdout  io.Writer
	Context io.ReadCloser
	// When closed, the job has been cancelled.
	// Note: not all jobs implement cancellation.
	// See Job.Cancel() and Job.WaitCancelled()
	cancelled  chan struct{}
	cancelOnce sync.Once
}

// Cancel signals the build job to cancel
func (b *Config) Cancel() {
	b.cancelOnce.Do(func() {
		close(b.cancelled)
	})
}

// WaitCancelled returns a channel which is closed ("never blocks") when
// the job is cancelled.
func (b *Config) WaitCancelled() <-chan struct{} {
	return b.cancelled
}

// NewBuildConfig returns a new Config struct
func NewBuildConfig() *Config {
	return &Config{
		AuthConfigs: map[string]cliconfig.AuthConfig{},
		cancelled:   make(chan struct{}),
	}
}

// Build is the main interface of the package, it gathers the Builder
// struct and calls builder.Run() to do all the real build job.
func Build(d *daemon.Daemon, buildConfig *Config) error {
	var (
		repoName string
		tag      string
		context  io.ReadCloser
	)
	sf := streamformatter.NewJSONStreamFormatter()

	repoName, tag = parsers.ParseRepositoryTag(buildConfig.RepoName)
	if repoName != "" {
		if err := registry.ValidateRepositoryName(repoName); err != nil {
			return err
		}
		if len(tag) > 0 {
			if err := tags.ValidateTagName(tag); err != nil {
				return err
			}
		}
	}

	if buildConfig.RemoteURL == "" {
		context = ioutil.NopCloser(buildConfig.Context)
	} else if urlutil.IsGitURL(buildConfig.RemoteURL) {
		root, err := utils.GitClone(buildConfig.RemoteURL)
		if err != nil {
			return err
		}
		defer os.RemoveAll(root)

		c, err := archive.Tar(root, archive.Uncompressed)
		if err != nil {
			return err
		}
		context = c
	} else if urlutil.IsURL(buildConfig.RemoteURL) {
		f, err := httputils.Download(buildConfig.RemoteURL)
		if err != nil {
			return fmt.Errorf("Error downloading remote context %s: %v", buildConfig.RemoteURL, err)
		}
		defer f.Body.Close()
		ct := f.Header.Get("Content-Type")
		clen := f.ContentLength
		contentType, bodyReader, err := inspectResponse(ct, f.Body, clen)

		defer bodyReader.Close()

		if err != nil {
			return fmt.Errorf("Error detecting content type for remote %s: %v", buildConfig.RemoteURL, err)
		}
		if contentType == httputils.MimeTypes.TextPlain {
			dockerFile, err := ioutil.ReadAll(bodyReader)
			if err != nil {
				return err
			}

			// When we're downloading just a Dockerfile put it in
			// the default name - don't allow the client to move/specify it
			buildConfig.DockerfileName = api.DefaultDockerfileName

			c, err := archive.Generate(buildConfig.DockerfileName, string(dockerFile))
			if err != nil {
				return err
			}
			context = c
		} else {
			// Pass through - this is a pre-packaged context, presumably
			// with a Dockerfile with the right name inside it.
			prCfg := progressreader.Config{
				In:        bodyReader,
				Out:       buildConfig.Stdout,
				Formatter: sf,
				Size:      clen,
				NewLines:  true,
				ID:        "Downloading context",
				Action:    buildConfig.RemoteURL,
			}
			context = progressreader.New(prCfg)
		}
	}

	defer context.Close()

	builder := &builder{
		Daemon: d,
		OutStream: &streamformatter.StdoutFormatter{
			Writer:          buildConfig.Stdout,
			StreamFormatter: sf,
		},
		ErrStream: &streamformatter.StderrFormatter{
			Writer:          buildConfig.Stdout,
			StreamFormatter: sf,
		},
		Verbose:         !buildConfig.SuppressOutput,
		UtilizeCache:    !buildConfig.NoCache,
		Remove:          buildConfig.Remove,
		ForceRemove:     buildConfig.ForceRemove,
		Pull:            buildConfig.Pull,
		OutOld:          buildConfig.Stdout,
		StreamFormatter: sf,
		AuthConfigs:     buildConfig.AuthConfigs,
		dockerfileName:  buildConfig.DockerfileName,
		cpuShares:       buildConfig.CPUShares,
		cpuPeriod:       buildConfig.CPUPeriod,
		cpuQuota:        buildConfig.CPUQuota,
		cpuSetCpus:      buildConfig.CPUSetCpus,
		cpuSetMems:      buildConfig.CPUSetMems,
		cgroupParent:    buildConfig.CgroupParent,
		memory:          buildConfig.Memory,
		memorySwap:      buildConfig.MemorySwap,
		ulimits:         buildConfig.Ulimits,
		cancelled:       buildConfig.WaitCancelled(),
		id:              stringid.GenerateRandomID(),
	}

	defer func() {
		builder.Daemon.Graph().Release(builder.id, builder.activeImages...)
	}()

	id, err := builder.Run(context)
	if err != nil {
		return err
	}
	if repoName != "" {
		return d.Repositories().Tag(repoName, tag, id, true)
	}
	return nil
}

// BuildFromConfig will do build directly from parameter 'changes', which comes
// from Dockerfile entries, it will:
//
// - call parse.Parse() to get AST root from Dockerfile entries
// - do build by calling builder.dispatch() to call all entries' handling routines
func BuildFromConfig(d *daemon.Daemon, c *runconfig.Config, changes []string) (*runconfig.Config, error) {
	ast, err := parser.Parse(bytes.NewBufferString(strings.Join(changes, "\n")))
	if err != nil {
		return nil, err
	}

	// ensure that the commands are valid
	for _, n := range ast.Children {
		if !validCommitCommands[n.Value] {
			return nil, fmt.Errorf("%s is not a valid change command", n.Value)
		}
	}

	builder := &builder{
		Daemon:        d,
		Config:        c,
		OutStream:     ioutil.Discard,
		ErrStream:     ioutil.Discard,
		disableCommit: true,
	}

	for i, n := range ast.Children {
		if err := builder.dispatch(i, n); err != nil {
			return nil, err
		}
	}

	return builder.Config, nil
}

// CommitConfig contains build configs for commit operation
type CommitConfig struct {
	Pause   bool
	Repo    string
	Tag     string
	Author  string
	Comment string
	Changes []string
	Config  *runconfig.Config
}

// Commit will create a new image from a container's changes
func Commit(name string, d *daemon.Daemon, c *CommitConfig) (string, error) {
	container, err := d.Get(name)
	if err != nil {
		return "", err
	}

	// It is not possible to commit a running container on Windows
	if runtime.GOOS == "windows" && container.IsRunning() {
		return "", fmt.Errorf("Windows does not support commit of a running container")
	}

	if c.Config == nil {
		c.Config = &runconfig.Config{}
	}

	newConfig, err := BuildFromConfig(d, c.Config, c.Changes)
	if err != nil {
		return "", err
	}

	if err := runconfig.Merge(newConfig, container.Config); err != nil {
		return "", err
	}

	commitCfg := &daemon.ContainerCommitConfig{
		Pause:   c.Pause,
		Repo:    c.Repo,
		Tag:     c.Tag,
		Author:  c.Author,
		Comment: c.Comment,
		Config:  newConfig,
	}

	img, err := d.Commit(container, commitCfg)
	if err != nil {
		return "", err
	}

	return img.ID, nil
}

// inspectResponse looks into the http response data at r to determine whether its
// content-type is on the list of acceptable content types for remote build contexts.
// This function returns:
//    - a string representation of the detected content-type
//    - an io.Reader for the response body
//    - an error value which will be non-nil either when something goes wrong while
//      reading bytes from r or when the detected content-type is not acceptable.
func inspectResponse(ct string, r io.ReadCloser, clen int64) (string, io.ReadCloser, error) {
	plen := clen
	if plen <= 0 || plen > maxPreambleLength {
		plen = maxPreambleLength
	}

	preamble := make([]byte, plen, plen)
	rlen, err := r.Read(preamble)
	if rlen == 0 {
		return ct, r, errors.New("Empty response")
	}
	if err != nil && err != io.EOF {
		return ct, r, err
	}

	preambleR := bytes.NewReader(preamble)
	bodyReader := ioutil.NopCloser(io.MultiReader(preambleR, r))
	// Some web servers will use application/octet-stream as the default
	// content type for files without an extension (e.g. 'Dockerfile')
	// so if we receive this value we better check for text content
	contentType := ct
	if len(ct) == 0 || ct == httputils.MimeTypes.OctetStream {
		contentType, _, err = httputils.DetectContentType(preamble)
		if err != nil {
			return contentType, bodyReader, err
		}
	}

	contentType = selectAcceptableMIME(contentType)
	var cterr error
	if len(contentType) == 0 {
		cterr = fmt.Errorf("unsupported Content-Type %q", ct)
		contentType = ct
	}

	return contentType, bodyReader, cterr
}
