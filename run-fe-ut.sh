#!/usr/bin/env bash
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

set -eo pipefail

ROOT=`dirname "$0"`
ROOT=`cd "$ROOT"; pwd`

export STARROCKS_HOME=${ROOT}

. ${STARROCKS_HOME}/env.sh

# Check args
usage() {
  echo "
Usage: $0 <options>
  Optional options:
     --clean    clean and build ut
     --run    build and run ut

  Eg.
    $0                      build and run ut
    $0 --coverage           build and run coverage statistic
    $0 --run xxx            build and run the specified class
  "
  exit 1
}

OPTS=$(getopt \
  -n $0 \
  -o '' \
  -l 'coverage' \
  -l 'run' \
  -l 'help' \
  -- "$@")

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$OPTS"

HELP=0
RUN=0
COVERAGE=0
while true; do 
    case "$1" in
        --coverage) COVERAGE=1 ; shift ;;
        --run) RUN=1 ; shift ;;
        --help) HELP=1 ; shift ;; 
        --) shift ;  break ;;
        *) ehco "Internal error" ; exit 1 ;;
    esac
done

if [ ${HELP} -eq 1 ]; then
    usage
    exit 0
fi

echo "Build Frontend UT"

echo "******************************"
echo "    Runing StarRocksFE Unittest    "
echo "******************************"

cd ${STARROCKS_HOME}/fe/
mkdir -p build/compile

if [ -z "${FE_UT_PARALLEL}" ]; then
    # the default fe unit test parallel is 1
    export FE_UT_PARALLEL=4
fi
echo "Unit test parallel is: $FE_UT_PARALLEL"

if [ -d "./mocked" ]; then
    rm -r ./mocked
fi

if [ -d "./ut_ports" ]; then
    rm -r ./ut_ports
fi

mkdir ut_ports

if [ ${COVERAGE} -eq 1 ]; then
    echo "Run coverage statistic"
    ant cover-test
else
    if [ ${RUN} -eq 1 ]; then
        echo "Run the specified class: $1"
        # eg:
        # sh run-fe-ut.sh --run com.starrocks.utframe.Demo
        # sh run-fe-ut.sh --run com.starrocks.utframe.Demo#testCreateDbAndTable+test2
        # set trimStackTrace to false to show full stack when debugging specified class or case
        ${MVN_CMD} test -DfailIfNoTests=false -DtrimStackTrace=false -D test=$1
    else    
        echo "Run Frontend UT"
        ${MVN_CMD} test -DfailIfNoTests=false -DtrimStackTrace=false
    fi 
fi
