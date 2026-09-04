#!/usr/bin/env bash
set -euo pipefail

as_json=0
case "$#" in
    0) ;;
    1) [[ "$1" == --json ]] || { echo "usage: $0 [--json]" >&2; exit 2; }; as_json=1 ;;
    *) echo "usage: $0 [--json]" >&2; exit 2 ;;
esac

root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "[ERR] project detection requires a Git repository" >&2
    exit 3
}
[[ "$(pwd -P)" == "$(CDPATH= cd -- "$root" && pwd -P)" ]] || {
    echo "[ERR] run project detection from the Git repository root" >&2
    exit 3
}

declare -a adapters=() roots=() evidence=() confidence=() reasons=() capabilities=()
declare -A seen=() primary=() hint_seen=()
declare -a hint_kinds=() hint_paths=() hint_reasons=()
evidence_separator=$'\034'

safe_path() {
    local value=$1
    local LC_ALL=C
    [[ -n "$value" && "$value" != /* && "$value" != *'\'* &&
       "$value" != ../* && "$value" != */../* ]] &&
        [[ ! "$value" =~ [[:cntrl:]] ]]
}

is_ignored_candidate_root() {
    local value="/${1#./}/"
    [[ "$value" == */.git/* || "$value" == */.cache/* || "$value" == */build/* ||
       "$value" == */cmake-build-*/* || "$value" == */_build/* ||
       "$value" == */_deps/* || "$value" == */.xmake/* ||
       "$value" == */out/* || "$value" == */dist/* || "$value" == */vendor/* ||
       "$value" == */third_party/* || "$value" == */external/* || "$value" == */deps/* ||
       "$value" == */node_modules/* ]]
}

file_root() {
    local file=$1 dir
    dir=${file%/*}
    [[ "$dir" != "$file" ]] || dir=.
    printf '%s' "$dir"
}

safe_candidate_root() {
    local value=$1 cursor=$1
    safe_path "$value" && ! is_ignored_candidate_root "$value" || return 1
    while :; do
        [[ -d "$cursor" && ! -L "$cursor" ]] || return 1
        [[ "$cursor" != . ]] || break
        if [[ "$cursor" == */* ]]; then cursor=${cursor%/*}; else cursor=.; fi
    done
}

eligible_file() {
    local value=$1 parent
    parent=$(file_root "$value")
    safe_path "$value" && ! is_ignored_candidate_root "$parent" &&
        [[ -f "$value" && ! -L "$value" ]]
}

confidence_rank() {
    case "$1" in high) printf 3 ;; medium) printf 2 ;; *) printf 1 ;; esac
}

merge_capabilities() {
    local existing=$1 incoming=$2 item result=$1
    IFS=, read -r -a incoming_items <<< "$incoming"
    for item in "${incoming_items[@]}"; do
        [[ -n "$item" ]] || continue
        case ",$result," in *",$item,"*) ;; *) result=${result:+$result,}$item ;; esac
    done
    printf '%s' "$result"
}

add_candidate() {
    local adapter=$1 module_root=$2 source=$3 level=$4 why=$5 caps=$6 key index current
    safe_candidate_root "$module_root" && eligible_file "$source" || return 0
    key="$adapter"$'\x1f'"$module_root"
    if [[ -n "${seen[$key]+x}" ]]; then
        index=${seen[$key]}
        case "$evidence_separator${evidence[$index]}$evidence_separator" in
            *"$evidence_separator$source$evidence_separator"*) ;;
            *) evidence[$index]+="$evidence_separator$source" ;;
        esac
        capabilities[$index]=$(merge_capabilities "${capabilities[$index]}" "$caps")
        if [[ $(confidence_rank "$level") -gt $(confidence_rank "${confidence[$index]}") ]]; then
            confidence[$index]=$level
            reasons[$index]=$why
        fi
        return 0
    fi
    index=${#adapters[@]}
    seen[$key]=$index
    adapters+=("$adapter")
    roots+=("$module_root")
    evidence+=("$source")
    confidence+=("$level")
    reasons+=("$why")
    capabilities+=("$caps")
}

add_hint() {
    local kind=$1 path=$2 why=$3 key
    eligible_file "$path" || return 0
    key="$kind"$'\x1f'"$path"
    [[ -z "${hint_seen[$key]+x}" ]] || return 0
    hint_seen[$key]=1
    hint_kinds+=("$kind")
    hint_paths+=("$path")
    hint_reasons+=("$why")
}

# Primary project descriptions establish candidate roots. Package, fragment
# and generated files are attached to the nearest primary root in the second
# pass instead of being promoted to independent high-confidence modules.
while IFS= read -r -d '' file; do
    eligible_file "$file" || continue
    base=${file##*/}
    module_root=$(file_root "$file")
    case "$base" in
        CMakeLists.txt) primary["cmake"$'\x1f'"$module_root"]=1 ;;
        meson.build) primary["meson"$'\x1f'"$module_root"]=1 ;;
        MODULE.bazel|WORKSPACE|WORKSPACE.bazel) primary["bazel"$'\x1f'"$module_root"]=1 ;;
        configure.ac|configure.in|configure) primary["autotools"$'\x1f'"$module_root"]=1 ;;
        xmake.lua) primary["xmake"$'\x1f'"$module_root"]=1 ;;
        *.pro) primary["qmake"$'\x1f'"$module_root"]=1 ;;
    esac
done < <(git ls-files -co --exclude-standard -z)

nearest_primary() {
    local adapter=$1 cursor=$2 key
    while :; do
        key="$adapter"$'\x1f'"$cursor"
        if [[ -n "${primary[$key]+x}" ]]; then printf '%s' "$cursor"; return 0; fi
        [[ "$cursor" != . ]] || return 1
        if [[ "$cursor" == */* ]]; then cursor=${cursor%/*}; else cursor=.; fi
    done
}

parent_root() {
    local value=$1
    [[ "$value" != . ]] || return 1
    if [[ "$value" == */* ]]; then value=${value%/*}; else value=.; fi
    printf '%s' "$value"
}

while IFS= read -r -d '' file; do
    eligible_file "$file" || continue
    base=${file##*/}
    module_root=$(file_root "$file")
    case "$base" in
        CMakeLists.txt)
            ancestor=
            if parent=$(parent_root "$module_root"); then ancestor=$(nearest_primary cmake "$parent" || true); fi
            if [[ -n "$ancestor" ]]; then
                add_candidate cmake "$module_root" "$file" low \
                    "Nested CMake description found below $ancestor; confirm whether it is a module or a subdirectory." \
                    "configure,build,test,install,consumer"
            else
                add_candidate cmake "$module_root" "$file" high \
                    "CMake project description found; generator, build directory and tests remain unconfirmed." \
                    "configure,build,test,install,consumer"
            fi
            ;;
        meson.build)
            ancestor=
            if parent=$(parent_root "$module_root"); then ancestor=$(nearest_primary meson "$parent" || true); fi
            if [[ -n "$ancestor" ]]; then
                add_candidate meson "$module_root" "$file" low \
                    "Nested Meson description found below $ancestor; confirm whether it is a module or subdir()." \
                    "configure,build,test,install"
            else
                add_candidate meson "$module_root" "$file" high \
                    "Meson project description found; backend and command options remain unconfirmed." \
                    "configure,build,test,install"
            fi
            ;;
        MODULE.bazel|WORKSPACE|WORKSPACE.bazel)
            add_candidate bazel "$module_root" "$file" high \
                "Bazel workspace metadata found; package and target scope remain unconfirmed." \
                "build,test,consumer"
            ;;
        MODULE.bazel.lock)
            owner_root=$(nearest_primary bazel "$module_root" || true)
            [[ -z "$owner_root" ]] || add_candidate bazel "$owner_root" "$file" high \
                "Bazel workspace metadata found; package and target scope remain unconfirmed." \
                "build,test,consumer"
            ;;
        BUILD|BUILD.bazel)
            owner_root=$(nearest_primary bazel "$module_root" || true)
            if [[ -n "$owner_root" ]]; then
                add_candidate bazel "$owner_root" "$file" high \
                    "Bazel workspace metadata found; package and target scope remain unconfirmed." \
                    "build,test,consumer"
                add_hint bazel-package "$file" "Bazel package metadata belongs to workspace root $owner_root and is not an independent module."
            elif [[ "$module_root" == . ]]; then
                add_candidate bazel . "$file" medium \
                    "Root Bazel package metadata found without MODULE/WORKSPACE; workspace identity requires review." \
                    "build,test,consumer"
            else
                add_hint bazel-package "$file" "Bazel package metadata has no reviewed workspace root and is not promoted to a module."
            fi
            ;;
        configure.ac|configure.in)
            add_candidate autotools "$module_root" "$file" high \
                "Autotools source metadata found; generated configure/Makefile commands are not executed." \
                "configure,build,test,install"
            ;;
        configure)
            add_candidate autotools "$module_root" "$file" medium \
                "Generated configure entry found; Autotools provenance and supported make targets require review." \
                "configure,build,test,install"
            ;;
        Makefile.am|Makefile.in)
            owner_root=$(nearest_primary autotools "$module_root" || true)
            if [[ -n "$owner_root" ]]; then
                add_candidate autotools "$owner_root" "$file" high \
                    "Autotools source metadata found; generated configure/Makefile commands are not executed." \
                    "configure,build,test,install"
                add_hint autotools-fragment "$file" "Autotools make fragment belongs to project root $owner_root."
            else
                add_hint autotools-fragment "$file" "Autotools make fragment has no configure.ac/configure.in ancestor."
            fi
            ;;
        Makefile|GNUmakefile|makefile)
            owner_root=$(nearest_primary autotools "$module_root" || true)
            if [[ -n "$owner_root" ]]; then
                add_candidate autotools "$owner_root" "$file" high \
                    "Autotools source or generated Make entry found; configure ownership requires review." \
                    "configure,build,test,install"
            else
                add_candidate make "$module_root" "$file" medium \
                    "Make entry found; available targets and side effects require review." \
                    "build,test,install"
            fi
            ;;
        xmake.lua)
            ancestor=
            if parent=$(parent_root "$module_root"); then ancestor=$(nearest_primary xmake "$parent" || true); fi
            if [[ -n "$ancestor" ]]; then
                add_candidate xmake "$module_root" "$file" low \
                    "Nested xmake description found below $ancestor; confirm independent module ownership." \
                    "configure,build,test,target-run"
            else
                add_candidate xmake "$module_root" "$file" high \
                    "xmake project description found; package-download behavior remains unconfirmed." \
                    "configure,build,test,target-run"
            fi
            ;;
        *.pro)
            add_candidate qmake "$module_root" "$file" medium \
                "qmake metadata found; Qt version and execution backend remain unconfirmed." \
                "configure,build,test,install"
            ;;
        *.pri)
            owner_root=$(nearest_primary qmake "$module_root" || true)
            if [[ -n "$owner_root" ]]; then
                add_candidate qmake "$owner_root" "$file" medium \
                    "qmake project metadata found; Qt version and execution backend remain unconfirmed." \
                    "configure,build,test,install"
                add_hint qmake-fragment "$file" "qmake include fragment belongs to project root $owner_root."
            else
                add_hint qmake-fragment "$file" "qmake include fragment has no .pro project ancestor."
            fi
            ;;
        build.ninja)
            add_candidate ninja "$module_root" "$file" low \
                "Ninja graph found, but it may be generated and is not promoted to project truth." \
                "build,test"
            ;;
        build.sh|build-project|configure-project|run-tests|test.sh)
            case "$file" in
                scripts/*|tools/*) custom_root=. ;;
                */scripts/*) custom_root=${file%%/scripts/*} ;;
                */tools/*) custom_root=${file%%/tools/*} ;;
                *) custom_root= ;;
            esac
            case "$base" in
                configure-project) custom_caps=configure ;;
                run-tests|test.sh) custom_caps=test ;;
                *) custom_caps=build ;;
            esac
            [[ -z "${custom_root:-}" ]] || add_candidate custom "$custom_root" "$file" low \
                "Repository script name suggests a $custom_caps command; content and side effects require review." \
                "$custom_caps"
            ;;
    esac
    case "$file" in
        .github/workflows/*.yml|.github/workflows/*.yaml|.gitlab-ci.yml|Jenkinsfile|.circleci/config.yml)
            add_hint ci "$file" "CI configuration is a review hint only and is never executed by detection."
            ;;
        CMakePresets.json|CMakeUserPresets.json|*/CMakePresets.json|*/CMakeUserPresets.json)
            add_hint cmake-preset "$file" "CMake preset is a review hint and is never executed by detection."
            ;;
        meson.options|meson_options.txt|*/meson.options|*/meson_options.txt)
            add_hint meson-options "$file" "Meson options are review hints and never become commands automatically."
            ;;
    esac
done < <(git ls-files -co --exclude-standard -z)

json_quote() {
    local value=$1
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//$'\n'/\\n}
    value=${value//$'\r'/\\r}
    value=${value//$'\t'/\\t}
    printf '"%s"' "$value"
}

json_string_array() {
    local csv=$1 first=1 item
    printf '['
    IFS=, read -r -a items <<< "$csv"
    for item in "${items[@]}"; do
        [[ -n "$item" ]] || continue
        [[ "$first" -eq 1 ]] || printf ','
        json_quote "$item"
        first=0
    done
    printf ']'
}

json_evidence_array() {
    local joined=$1 first=1 item
    printf '['
    IFS="$evidence_separator" read -r -a items <<< "$joined"
    for item in "${items[@]}"; do
        [[ -n "$item" ]] || continue
        [[ "$first" -eq 1 ]] || printf ','
        json_quote "$item"
        first=0
    done
    printf ']'
}

ordered_indices() {
    local i
    for i in "${!adapters[@]}"; do
        printf '%s\t%s\t%08d\n' "${roots[$i]}" "${adapters[$i]}" "$i"
    done | LC_ALL=C sort | awk -F '\t' '{print $3+0}'
}

if [[ "$as_json" -eq 1 ]]; then
    printf '{\n  "schema_version": 1,\n  "repository_root": ".",\n  "selected": null,\n  "candidates": ['
    first=1
    while IFS= read -r i; do
        [[ -n "$i" ]] || continue
        [[ "$first" -eq 1 ]] || printf ','
        printf '\n    {"adapter":'
        json_quote "${adapters[$i]}"
        printf ',"module_root":'
        json_quote "${roots[$i]}"
        printf ',"evidence":'
        json_evidence_array "${evidence[$i]}"
        printf ',"confidence":'
        json_quote "${confidence[$i]}"
        printf ',"capability_candidates":'
        json_string_array "${capabilities[$i]}"
        printf ',"reason":'
        json_quote "${reasons[$i]}"
        printf ',"requires_human_confirmation":true}'
        first=0
    done < <(ordered_indices)
    [[ "$first" -eq 1 ]] || printf '\n  '
    printf '],\n  "hints": ['
    first=1
    for i in "${!hint_paths[@]}"; do
        [[ "$first" -eq 1 ]] || printf ','
        printf '\n    {"kind":'
        json_quote "${hint_kinds[$i]}"
        printf ',"path":'
        json_quote "${hint_paths[$i]}"
        printf ',"reason":'
        json_quote "${hint_reasons[$i]}"
        printf '}'
        first=0
    done
    [[ "$first" -eq 1 ]] || printf '\n  '
    printf '],\n  "side_effects": []\n}\n'
    exit 0
fi

echo "Project detection candidates (read-only; nothing was selected):"
if [[ "${#adapters[@]}" -eq 0 ]]; then
    echo "- No built-in candidate found. A reviewed custom Project Profile can still describe this repository."
else
    display_index=0
    while IFS= read -r i; do
        [[ -n "$i" ]] || continue
        display_index=$((display_index + 1))
        printf -- '- [%d] module=%s adapter=%s confidence=%s evidence=%s\n' \
            "$display_index" "${roots[$i]}" "${adapters[$i]}" "${confidence[$i]}" "${evidence[$i]%%"$evidence_separator"*}"
        printf '  %s\n' "${reasons[$i]}"
    done < <(ordered_indices)
fi
for i in "${!hint_paths[@]}"; do
    printf -- '- hint=%s path=%s (%s)\n' "${hint_kinds[$i]}" "${hint_paths[$i]}" "${hint_reasons[$i]}"
done
echo "Detection did not run a build, install dependencies, access a registry or modify the worktree."
