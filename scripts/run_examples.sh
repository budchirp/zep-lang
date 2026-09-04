#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

readonly RED="\033[0;31m"
readonly GREEN="\033[0;32m"
readonly YELLOW="\033[1;33m"
readonly BLUE="\033[0;34m"
readonly CYAN="\033[0;36m"
readonly DIM="\033[2m"
readonly BOLD="\033[1m"
readonly NC="\033[0m"

BUILD_DIR="${REPO_ROOT}/cmake-build-debug"

if [[ ! -d "$BUILD_DIR" ]]; then
    BUILD_DIR="${REPO_ROOT}/cmake-build-release"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}error:${NC} no build directory found"
    echo -e "${DIM}  cmake --preset debug && cmake --build cmake-build-debug${NC}"
    exit 1
fi

ZEP_DEBUG="${REPO_ROOT}/cmake-build-debug/cli/zep"
ZEP_RELEASE="${REPO_ROOT}/cmake-build-release/cli/zep"

if [[ -f "$ZEP_DEBUG" ]]; then
    ZEP_EXE="$ZEP_DEBUG"
elif [[ -f "$ZEP_RELEASE" ]]; then
    ZEP_EXE="$ZEP_RELEASE"
else
    echo -e "${RED}error:${NC} zep executable not found"
    echo -e "${DIM}  cmake --preset debug && cmake --build cmake-build-debug${NC}"
    exit 1
fi

readonly EXAMPLES_DIR="${REPO_ROOT}/examples"

mapfile -t projects < <(find "$EXAMPLES_DIR" -mindepth 2 -maxdepth 2 -name "zep.json" -printf "%h\n" | sort)

if [[ "${#projects[@]}" -eq 0 ]]; then
    echo -e "${YELLOW}warning:${NC} no example projects found"
    exit 0
fi

echo -e "${BOLD}${BLUE}Zep examples${NC} ${DIM}(${EXAMPLES_DIR})${NC}"
echo -e "${DIM}────────────────────────────────────────────────────────────${NC}"

failed=0

for project_dir in "${projects[@]}"; do
    project_name="$(basename "$project_dir")"
    binary_path="${project_dir}/build/${project_name}"

    echo -e "${CYAN}${BOLD}${project_name}${NC}"

    if grep -q '"type"[[:space:]]*:[[:space:]]*"library"' "${project_dir}/zep.json"; then
        echo -e "  ${YELLOW}skipped${NC} ${DIM}(library project)${NC}"
        echo
        continue
    fi

    echo -e "${DIM}  building...${NC}"

    build_log=$(mktemp)
    if ! (cd "$project_dir" && LLVM_PROFILE_FILE="${BUILD_DIR}/coverage/%p.profraw" "$ZEP_EXE" build) >"$build_log" 2>&1; then
        echo -e "  ${RED}build failed${NC}"
        sed 's/^/  /' "$build_log"
        rm -f "$build_log"
        failed=1
        echo
        continue
    fi
    rm -f "$build_log"

    if [[ ! -x "$binary_path" ]]; then
        echo -e "  ${RED}run failed${NC} ${DIM}(missing ${binary_path})${NC}"
        failed=1
        echo
        continue
    fi

    echo -e "${DIM}  running...${NC}"
    timeout 5 "$binary_path"
    run_ec=$?

    if [[ "$run_ec" -ne 0 ]]; then
        echo -e "  ${RED}run failed${NC} ${DIM}(exit ${run_ec})${NC}"
        failed=1
        echo
        continue
    fi

    echo -e "  ${GREEN}succeeded${NC}"
    echo
done

echo -e "${DIM}────────────────────────────────────────────────────────────${NC}"
if [[ "$failed" -eq 0 ]]; then
    echo -e "${GREEN}${BOLD}All examples succeeded.${NC}"
else
    echo -e "${RED}${BOLD}Some examples failed.${NC}"
    exit 1
fi
