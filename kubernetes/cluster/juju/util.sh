#!/bin/bash

# Copyright 2015 The Kubernetes Authors All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


set -o errexit
set -o nounset
set -o pipefail

UTIL_SCRIPT=$(readlink -m "${BASH_SOURCE}")
JUJU_PATH=$(dirname ${UTIL_SCRIPT})
source ${JUJU_PATH}/prereqs/ubuntu-juju.sh
export JUJU_REPOSITORY=${JUJU_PATH}/charms
#KUBE_BUNDLE_URL='https://raw.githubusercontent.com/whitmo/bundle-kubernetes/master/bundles.yaml'
KUBE_BUNDLE_PATH=${JUJU_PATH}/bundles/local.yaml

function verify-prereqs() {
    gather_installation_reqs
}

function build-local() {
    # Make a clean environment to avoid compiler errors.
    make clean
    # Build the binaries locally that are used in the charms.
    make all WHAT="cmd/kube-apiserver cmd/kubectl cmd/kube-controller-manager plugin/cmd/kube-scheduler cmd/kubelet cmd/kube-proxy"
    OUTPUT_DIR=_output/local/bin/linux/amd64
    mkdir -p cluster/juju/charms/trusty/kubernetes-master/files/output
    # Copy the binary output to the charm directory.
    cp -v $OUTPUT_DIR/* cluster/juju/charms/trusty/kubernetes-master/files/output
}

function kube-up() {
    build-local
    if [[ -d "~/.juju/current-env" ]]; then
        juju quickstart -i --no-browser
    else
        juju quickstart --no-browser
    fi
    # The juju-deployer command will deploy the bundle and can be run
    # multiple times to continue deploying the parts that fail.
    juju deployer -c ${KUBE_BUNDLE_PATH}
    # Sleep due to juju bug http://pad.lv/1432759
    sleep-status
    detect-master
    detect-minions

    export KUBE_MASTER_IP="${KUBE_MASTER_IP}:8080"
    export CONTEXT="juju"
}

function kube-down() {
    # Remove the binary files from the charm directory.
    rm -rf cluster/juju/charms/trusty/kubernetes-master/files/output/
    local jujuenv
    jujuenv=$(cat ~/.juju/current-environment)
    juju destroy-environment $jujuenv
}

function detect-master() {
    local kubestatus
    # Capturing a newline, and my awk-fu was weak - pipe through tr -d
    kubestatus=$(juju status --format=oneline kubernetes-master | grep kubernetes-master/0 | awk '{print $3}' | tr -d "\n")
    export KUBE_MASTER_IP=${kubestatus}
    export KUBE_MASTER=${KUBE_MASTER_IP}
    export KUBERNETES_MASTER=http://${KUBE_MASTER}:8080
    echo "Kubernetes master: " ${KUBERNETES_MASTER}
}

function detect-minions() {
    # Run the Juju command that gets the minion private IP addresses.
    local ipoutput
    ipoutput=$(juju run --service kubernetes "unit-get private-address" --format=json)
    echo $ipoutput
    # Strip out the IP addresses
    #
    # Example Output:
    #- MachineId: "10"
    #  Stdout: |
    #    10.197.55.232
    # UnitId: kubernetes/0
    # - MachineId: "11"
    # Stdout: |
    #    10.202.146.124
    #  UnitId: kubernetes/1
    export KUBE_MINION_IP_ADDRESSES=($(${JUJU_PATH}/return-node-ips.py "${ipoutput}"))
    echo "Kubernetes minions:  " ${KUBE_MINION_IP_ADDRESSES[@]}
    export NUM_MINIONS=${#KUBE_MINION_IP_ADDRESSES[@]}
    export MINION_NAMES=$KUBE_MINION_IP_ADDRESSES
}

function setup-logging-firewall() {
    echo "TODO: setup logging and firewall rules"
}

function teardown-logging-firewall() {
    echo "TODO: teardown logging and firewall rules"
}

function sleep-status() {
    local i
    local maxtime
    local jujustatus
    i=0
    maxtime=900
    jujustatus=''
    echo "Waiting up to 15 minutes to allow the cluster to come online... wait for it..."

    jujustatus=$(juju status kubernetes-master --format=oneline)
    if [[ $jujustatus == *"started"* ]];
    then
        return
    fi

    while [[ $i < $maxtime && $jujustatus != *"started"* ]]; do
        sleep 15
        i+=15
        jujustatus=$(juju status kubernetes-master --format=oneline)
    done

    # sleep because we cannot get the status back of where the minions are in the deploy phase
    # thanks to a generic "started" state and our service not actually coming online until the
    # minions have received the binary from the master distribution hub during relations
    echo "Sleeping an additional minute to allow the cluster to settle"
    sleep 60
}
