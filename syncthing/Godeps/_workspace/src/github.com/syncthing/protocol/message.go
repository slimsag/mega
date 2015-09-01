// Copyright (C) 2014 The Protocol Authors.

//go:generate -command genxdr go run ../syncthing/Godeps/_workspace/src/github.com/calmh/xdr/cmd/genxdr/main.go
//go:generate genxdr -o message_xdr.go message.go

package protocol

import "fmt"

type IndexMessage struct {
	Folder  string
	Files   []FileInfo // max:1000000
	Flags   uint32
	Options []Option // max:64
}

type FileInfo struct {
	Name         string // max:8192
	Flags        uint32
	Modified     int64
	Version      Vector
	LocalVersion int64
	CachedSize   int64       // noencode (cache only)
	Blocks       []BlockInfo // max:1000000
}

func (f FileInfo) String() string {
	return fmt.Sprintf("File{Name:%q, Flags:0%o, Modified:%d, Version:%v, Size:%d, Blocks:%v}",
		f.Name, f.Flags, f.Modified, f.Version, f.Size(), f.Blocks)
}

func (f FileInfo) Size() (bytes int64) {
	if f.IsDeleted() || f.IsDirectory() {
		return 128
	}
	for _, b := range f.Blocks {
		bytes += int64(b.Size)
	}
	return
}

func (f FileInfo) IsDeleted() bool {
	return f.Flags&FlagDeleted != 0
}

func (f FileInfo) IsInvalid() bool {
	return f.Flags&FlagInvalid != 0
}

func (f FileInfo) IsDirectory() bool {
	return f.Flags&FlagDirectory != 0
}

func (f FileInfo) IsSymlink() bool {
	return f.Flags&FlagSymlink != 0
}

func (f FileInfo) HasPermissionBits() bool {
	return f.Flags&FlagNoPermBits == 0
}

// WinsConflict returns true if "f" is the one to choose when it is in
// conflict with "other".
func (f FileInfo) WinsConflict(other FileInfo) bool {
	// If a modification is in conflict with a delete, we pick the
	// modification.
	if !f.IsDeleted() && other.IsDeleted() {
		return true
	}
	if f.IsDeleted() && !other.IsDeleted() {
		return false
	}

	// The one with the newer modification time wins.
	if f.Modified > other.Modified {
		return true
	}
	if f.Modified < other.Modified {
		return false
	}

	// The modification times were equal. Use the device ID in the version
	// vector as tie breaker.
	return f.Version.Compare(other.Version) == ConcurrentGreater
}

type BlockInfo struct {
	Offset int64 // noencode (cache only)
	Size   int32
	Hash   []byte // max:64
}

func (b BlockInfo) String() string {
	return fmt.Sprintf("Block{%d/%d/%x}", b.Offset, b.Size, b.Hash)
}

type RequestMessage struct {
	Folder  string // max:64
	Name    string // max:8192
	Offset  int64
	Size    int32
	Hash    []byte // max:64
	Flags   uint32
	Options []Option // max:64
}

type ResponseMessage struct {
	Data []byte
	Code int32
}

type ClusterConfigMessage struct {
	ClientName    string   // max:64
	ClientVersion string   // max:64
	Folders       []Folder // max:1000000
	Options       []Option // max:64
}

func (o *ClusterConfigMessage) GetOption(key string) string {
	for _, option := range o.Options {
		if option.Key == key {
			return option.Value
		}
	}
	return ""
}

type Folder struct {
	ID      string   // max:64
	Devices []Device // max:1000000
	Flags   uint32
	Options []Option // max:64
}

type Device struct {
	ID              []byte // max:32
	MaxLocalVersion int64
	Flags           uint32
	Options         []Option // max:64
}

type Option struct {
	Key   string // max:64
	Value string // max:1024
}

type CloseMessage struct {
	Reason string // max:1024
	Code   int32
}

type EmptyMessage struct{}
