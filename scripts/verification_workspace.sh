#!/usr/bin/env bash
set -euo pipefail
if [[ $# -eq 1 && "$1" == --protocol ]]; then
    printf 'autoai-verification-workspace-v2\n'
    exit 0
fi
source "$(dirname "$0")/harness_lock.sh"

usage() {
    echo "usage: $0 run <change> -- <command...> | cleanup <change> | assert-clean <change>" >&2
    exit 2
}

[[ $# -ge 2 ]] || usage
action=$1
change=$2
shift 2
case "$action" in run|cleanup|assert-clean) ;; *) usage ;; esac
harness_validate_change_id "$change"
workspace_root=.ai-harness/logs/verification-workspaces
change_root="$workspace_root/$change"

require_managed_lock() {
    local effective_purpose lock_change lock_pid token_file=.ai-harness/locks/managed-operation.lock/verification-workspace-token
    harness_assert_repo_path .ai-harness/locks/managed-operation.lock dir || {
        echo '[ERR] verification workspace is available only inside a managed task/Evaluation lifecycle command' >&2
        return 4
    }
    for file in token change purpose pid "$token_file"; do
        [[ "$file" == "$token_file" ]] && {
            harness_assert_repo_path "$file" file || return 4
            continue
        }
        harness_assert_repo_path ".ai-harness/locks/managed-operation.lock/$file" file || return 4
    done
    lock_change=$(cat .ai-harness/locks/managed-operation.lock/change)
    lock_pid=$(cat .ai-harness/locks/managed-operation.lock/pid)
    [[ "$lock_change" == "$change" ]] || {
        echo '[ERR] verification workspace change does not match the managed lock' >&2
        return 4
    }
    [[ "$lock_pid" =~ ^[0-9]+$ && "$lock_pid" -gt 1 ]] && kill -0 "$lock_pid" 2>/dev/null || {
        echo '[ERR] stale managed lock cannot authorize verification workspace access' >&2
        return 4
    }
    [[ -n "${AUTOAI_VERIFICATION_WORKSPACE_TOKEN:-}" ]] || {
        echo '[ERR] verification workspace requires a one-shot capability from its managed parent' >&2
        return 4
    }
    effective_purpose=$(
        node - "$token_file" "$AUTOAI_VERIFICATION_WORKSPACE_TOKEN" "$action" "$change" \
            .ai-harness/locks/managed-operation.lock/token \
            .ai-harness/locks/managed-operation.lock/pid \
            .ai-harness/locks/managed-operation.lock/purpose \
            .ai-harness/locks/managed-operation.lock/change <<'NODE'
const fs=require('fs'),crypto=require('crypto'),[capFile,token,action,change,lockTokenFile,pidFile,purposeFile,changeFile]=process.argv.slice(2),sha=v=>'sha256:'+crypto.createHash('sha256').update(v).digest('hex'),uid=typeof process.geteuid==='function'?process.geteuid():null,readRegular=file=>{const st=fs.lstatSync(file);if(!st.isFile()||st.isSymbolicLink())throw Error('unsafe lock metadata');return {st,text:fs.readFileSync(file,'utf8').replace(/\n$/,'')}},capState=readRegular(capFile);if(uid!==null&&(capState.st.uid!==uid)||(capState.st.mode&0o077)!==0)throw Error('unsafe capability ownership');const d=JSON.parse(capState.text),keys=['schema_version','token_sha256','change_name','action','owner_purpose','effective_purpose','owner_pid','lock_token_sha256','issued_at'];if(!d||typeof d!=='object'||Array.isArray(d)||Object.keys(d).length!==keys.length||keys.some(k=>!Object.prototype.hasOwnProperty.call(d,k)))throw Error('capability schema mismatch');const lockToken=readRegular(lockTokenFile).text,pid=Number(readRegular(pidFile).text),ownerPurpose=readRegular(purposeFile).text,lockChange=readRegular(changeFile).text,issued=Date.parse(d.issued_at),now=Date.now();if(!/^[0-9a-f]{48}$/.test(token)||d.schema_version!==1||d.token_sha256!==sha(token)||d.change_name!==change||d.action!==action||d.owner_purpose!==ownerPurpose||typeof d.effective_purpose!=='string'||!d.effective_purpose||d.owner_pid!==pid||d.lock_token_sha256!==sha(lockToken)||lockChange!==change||!Number.isInteger(pid)||pid<=1||!Number.isFinite(issued)||issued<now-300000||issued>now+300000)throw Error('capability identity mismatch');try{process.kill(pid,0)}catch(e){if(e.code!=='EPERM')throw Error('stale lock owner')}const after=fs.lstatSync(capFile);if(after.isSymbolicLink()||!after.isFile()||after.dev!==capState.st.dev||after.ino!==capState.st.ino)throw Error('capability changed before consume');fs.unlinkSync(capFile);process.stdout.write(d.effective_purpose);
NODE
    ) || {
        echo '[ERR] invalid verification workspace capability' >&2
        return 4
    }
    unset AUTOAI_VERIFICATION_WORKSPACE_TOKEN
    case "$action:$effective_purpose" in
        run:task-verify|run:evaluation-run) ;;
        cleanup:task-verify|cleanup:task-verify-complete|cleanup:integration-refresh|cleanup:evaluation-run|cleanup:evaluation-begin|cleanup:evaluation-abort|cleanup:archive) ;;
        assert-clean:task-verify|assert-clean:task-verify-complete|assert-clean:integration-refresh|assert-clean:integration-check|assert-clean:evaluation-run|assert-clean:evaluation-begin|assert-clean:evaluation-finish|assert-clean:evaluation-abort|assert-clean:evaluation-complete-check|assert-clean:archive) ;;
        *) echo '[ERR] current managed operation is not authorized for this verification workspace action' >&2; return 4 ;;
    esac
}

prepare_workspace_root() {
    harness_prepare_runtime_dir logs || return 4
    harness_assert_repo_path "$workspace_root" dir-or-missing || return 4
    if [[ ! -e "$workspace_root" ]]; then
        mkdir -- "$workspace_root"
        chmod 700 "$workspace_root"
    fi
    harness_assert_repo_path "$workspace_root" dir || return 4
}

remove_workspace_path() {
    local relative=$1
    node - "$HARNESS_REPO_ROOT" "$relative" "$workspace_root" "$change" <<'NODE'
const fs=require('fs'),path=require('path'),cp=require('child_process');
const [rootArg,relative,workspaceRoot,change]=process.argv.slice(2),root=fs.realpathSync(rootArg);
const expectedChange=workspaceRoot+'/'+change;
if(relative!==expectedChange&&!relative.startsWith(expectedChange+'/run.'))throw Error('refusing to remove a non-workspace path');
const parts=relative.split('/');
let cursor=root;
for(let i=0;i<parts.length-1;i++){
  cursor=path.join(cursor,parts[i]);
  const st=fs.lstatSync(cursor);
  if(st.isSymbolicLink()||!st.isDirectory())throw Error('unsafe verification workspace ancestor');
}
const target=path.join(root,...parts);
let targetStat;try{targetStat=fs.lstatSync(target)}catch(e){if(e.code==='ENOENT')process.exit(0);throw e}
if(targetStat.isSymbolicLink()||!targetStat.isDirectory())throw Error('unsafe verification workspace target');
const tracked=cp.execFileSync('git',['ls-files','-z','--',relative],{cwd:root}).toString('utf8').split('\0').filter(Boolean);
if(tracked.length)throw Error('refusing to remove tracked verification workspace content: '+tracked.join(', '));
function removeTree(item){
  const st=fs.lstatSync(item);
  if(st.isSymbolicLink())throw Error('refusing to remove a symbolic link from the verification workspace');
  if(st.isDirectory()){
    for(const name of fs.readdirSync(item))removeTree(path.join(item,name));
    const after=fs.lstatSync(item);
    if(after.isSymbolicLink()||!after.isDirectory()||after.dev!==st.dev||after.ino!==st.ino)throw Error('verification workspace directory changed during cleanup');
    fs.rmdirSync(item);
  }else if(st.isFile()){
    fs.unlinkSync(item);
  }else throw Error('refusing to remove a special file from the verification workspace');
}
removeTree(target);
NODE
}

assert_workspace_clean() {
    node - "$HARNESS_REPO_ROOT" "$workspace_root" "$change" <<'NODE'
const fs=require('fs'),path=require('path');
const [rootArg,workspaceRoot,change]=process.argv.slice(2),root=fs.realpathSync(rootArg),parts=workspaceRoot.split('/');
let cursor=root;
for(let i=0;i<parts.length;i++){
  cursor=path.join(cursor,parts[i]);
  let st;try{st=fs.lstatSync(cursor)}catch(e){if(e.code==='ENOENT')process.exit(0);throw e}
  if(st.isSymbolicLink()||!st.isDirectory())throw Error('unsafe verification workspace ancestor');
}
const target=path.join(root,...parts,change);
let st;try{st=fs.lstatSync(target)}catch(e){if(e.code==='ENOENT')process.exit(0);throw e}
if(st.isSymbolicLink()||!st.isDirectory())throw Error('unsafe verification workspace target');
const entries=fs.readdirSync(target);
if(entries.length){console.error('[ERR] temporary verification workspace is not empty: '+entries.join(', '));process.exit(6)}
NODE
}

require_managed_lock
case "$action" in
    run)
        [[ "${1:-}" == -- ]] || usage
        shift
        [[ $# -gt 0 ]] || usage
        prepare_workspace_root
        harness_assert_repo_path "$change_root" dir-or-missing || exit 4
        if [[ ! -e "$change_root" ]]; then mkdir -- "$change_root"; chmod 700 "$change_root"; fi
        harness_assert_repo_path "$change_root" dir || exit 4
        run_dir=$(mktemp -d "$change_root/run.XXXXXX")
        run_rel=${run_dir#./}
        chmod 700 "$run_dir"
        cleanup_run() {
            local command_status=$?
            trap - EXIT INT TERM HUP
            if ! remove_workspace_path "$run_rel"; then
                echo '[ERR] temporary verification program cleanup failed; completion is blocked' >&2
                exit 6
            fi
            rmdir -- "$change_root" 2>/dev/null || true
            exit "$command_status"
        }
        trap cleanup_run EXIT
        trap 'exit 130' INT
        trap 'exit 143' TERM
        trap 'exit 129' HUP
        export AUTOAI_VERIFY_TMPDIR="$HARNESS_REPO_ROOT/$run_rel"
        export TMPDIR="$AUTOAI_VERIFY_TMPDIR"
        "$@"
        ;;
    cleanup)
        [[ $# -eq 0 ]] || usage
        if [[ -e "$change_root" || -L "$change_root" ]]; then remove_workspace_path "$change_root"; fi
        assert_workspace_clean
        ;;
    assert-clean)
        [[ $# -eq 0 ]] || usage
        assert_workspace_clean
        ;;
esac
