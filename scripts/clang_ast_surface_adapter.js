#!/usr/bin/env node
'use strict';

// Optional, fail-closed Clang AST discovery for Integration Completeness v1.
// The adapter never evaluates a compile database command string and never
// invokes a shell.  It deliberately supports a small, audited argv grammar;
// an input outside that grammar blocks the selected discovery mode.
const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');
const cp = require('child_process');
const { TextDecoder } = require('util');
const scopeLib = require('./change_scope.js');

const ADAPTER_ID = 'clang-ast-v1';
const ADAPTER_SCHEMA = 1;
const MAX_DATABASE_BYTES = 16 * 1024 * 1024;
const MAX_DATABASE_ENTRIES = 4096;
const MAX_ARGUMENTS = 2048;
const MAX_ARGUMENT_BYTES = 16384;
const MAX_AST_BYTES = 64 * 1024 * 1024;
const MAX_AST_INVOCATIONS = 512;
const MAX_BASE_BYTES = 512 * 1024 * 1024;
const MAX_BASE_FILES = 50000;
const TOOL_TIMEOUT_MS = 30000;
const TOTAL_DISCOVERY_TIMEOUT_MS = 5 * 60 * 1000;
let discoveryDeadline = null;
const utf8 = new TextDecoder('utf-8', { fatal: true });
const cmp = (a, b) => Buffer.from(a).compare(Buffer.from(b));
const canonical = value => Array.isArray(value) ? '[' + value.map(canonical).join(',') + ']'
    : value && typeof value === 'object' ? '{' + Object.keys(value).sort(cmp).map(k => JSON.stringify(k) + ':' + canonical(value[k])).join(',') + '}'
    : JSON.stringify(value);
const sha = value => 'sha256:' + crypto.createHash('sha256').update(value).digest('hex');
const block = message => { const error = Error(message); error.gateStatus = 'blocked'; throw error; };
const invalid = message => { const error = Error(message); error.gateStatus = 'invalid'; throw error; };
const normalizeType = value => {
    if (typeof value !== 'string' || !value.trim()) block('unsupported_or_missing_canonical_qualtype');
    return value.trim().replace(/\s+/g, ' ').replace(/\s*([*&<>,()[\]])\s*/g, '$1');
};
const isWithin = (root, value) => value === root || value.startsWith(root + path.sep);
const safeRel = (value, label) => {
    try { return scopeLib.safePath(value, label); } catch { invalid('unsafe_' + label.replace(/[^a-z0-9]+/gi, '_').toLowerCase()); }
};
const fileExists = file => { try { const st = fs.lstatSync(file); return st.isFile() && !st.isSymbolicLink(); } catch (error) { if (error.code === 'ENOENT') return false; throw error; } };
const headerPath = value => /(?:^|\/)(?:include|public|export)(?:\/|$)/.test(value) || /\.(?:h|hh|hpp|hxx)$/.test(value);
const sourcePath = value => /\.(?:cc|cpp|cxx|c\+\+|C)$/.test(value);
const remainingTimeout = label => {
    if (!Number.isFinite(discoveryDeadline)) return TOOL_TIMEOUT_MS;
    const remaining = discoveryDeadline - Date.now();
    if (remaining <= 0) block('ast_discovery_total_deadline_exceeded_' + label);
    return Math.max(1, Math.min(TOOL_TIMEOUT_MS, remaining));
};

function decode(buffer, label, reject = block) {
    try { return utf8.decode(buffer); } catch { reject(label + '_is_not_utf8'); }
}

function parseJsonNoDuplicates(source, label, reject = block) {
    let index = 0;
    const ws = () => { while (/\s/.test(source[index] || '')) index++; };
    const string = () => { const start = index++; for (; index < source.length; index++) { if (source[index] === '\\') { index++; continue; } if (source[index] === '"') { index++; try { return JSON.parse(source.slice(start, index)); } catch { reject(label + '_invalid_string'); } } } reject(label + '_unterminated_string'); };
    const value = () => {
        ws(); const c = source[index];
        if (c === '"') return string();
        if (c === '{') { index++; const out = {}, seen = new Set; ws(); if (source[index] === '}') { index++; return out; } for (;;) { ws(); if (source[index] !== '"') reject(label + '_key_expected'); const key = string(); if (seen.has(key)) reject(label + '_duplicate_key'); seen.add(key); ws(); if (source[index++] !== ':') reject(label + '_colon_expected'); out[key] = value(); ws(); const delimiter = source[index++]; if (delimiter === '}') return out; if (delimiter !== ',') reject(label + '_comma_expected'); } }
        if (c === '[') { index++; const out = []; ws(); if (source[index] === ']') { index++; return out; } for (;;) { out.push(value()); ws(); const delimiter = source[index++]; if (delimiter === ']') return out; if (delimiter !== ',') reject(label + '_comma_expected'); } }
        const match = source.slice(index).match(/^(?:true|false|null|-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?)/);
        if (!match) reject(label + '_invalid_json'); index += match[0].length; return JSON.parse(match[0]);
    };
    const result = value(); ws(); if (index !== source.length) reject(label + '_trailing_json'); return result;
}

function checkedRepoPath(root, directory, value, label, requireExisting = false) {
    if (typeof value !== 'string' || !value || value.includes('\0') || /[\r\n]/.test(value)) invalid('unsafe_' + label);
    const absolute = path.resolve(directory, value);
    if (!isWithin(root, absolute) || absolute === path.join(root, '.git') || absolute.startsWith(path.join(root, '.git') + path.sep)) invalid(label + '_outside_repository');
    const relative = path.relative(root, absolute).split(path.sep).join('/') || '.'; safeRel(relative, label);
    let cursor = root;
    for (const part of relative.split('/')) {
        cursor = path.join(cursor, part);
        try { if (fs.lstatSync(cursor).isSymbolicLink()) invalid(label + '_uses_symlink'); }
        catch (error) { if (error.code !== 'ENOENT') throw error; break; }
    }
    if (requireExisting && !fileExists(absolute)) invalid(label + '_is_not_regular_file');
    return relative;
}

function resolveClang() {
    const search = (process.env.PATH || '').split(path.delimiter).filter(Boolean);
    for (const directory of search) {
        const candidate = path.resolve(directory, 'clang++');
        try {
            const resolved = fs.realpathSync(candidate), st = fs.statSync(resolved);
            if (st.isFile() && (st.mode & 0o111) && path.isAbsolute(resolved)) return resolved;
        } catch (error) { if (!['ENOENT', 'EACCES'].includes(error.code)) block('clang_resolution_failed'); }
    }
    block('clang++_not_found');
}

function runTool(tool, args, cwd, tempRoot, maxBuffer = MAX_AST_BYTES) {
    if (!Array.isArray(args) || args.some(x => typeof x !== 'string' || x.includes('\0') || /[\r\n]/.test(x))) block('unsafe_clang_argv');
    const result = cp.spawnSync(tool, args, {
        cwd, encoding: null, timeout: remainingTimeout('clang'), maxBuffer,
        env: { HOME: tempRoot, LANG: 'C', LC_ALL: 'C', PATH: '/usr/bin:/bin', TMPDIR: tempRoot },
        windowsHide: true, shell: false
    });
    if (result.error?.code === 'ETIMEDOUT' && Date.now() >= discoveryDeadline) block('ast_discovery_total_deadline_exceeded_clang');
    if (result.signal) block('clang_terminated_by_signal_' + result.signal);
    if (result.status !== 0) block('clang_probe_or_parse_failed_exit_' + String(result.status));
    // Some constrained runtimes report a post-exit EPERM while still returning
    // a complete status-0 child result.  Only an error without successful child
    // completion is an execution failure; stdout is still size-bounded above.
    if (result.error && result.status !== 0) block('clang_execution_failed_' + (result.error.code || 'unknown'));
    return { stdout: result.stdout || Buffer.alloc(0), stderr: result.stderr || Buffer.alloc(0) };
}

function findNodes(root, kind, name) {
    const result = [];
    const walk = node => { if (!node || typeof node !== 'object') return; if ((!kind || node.kind === kind) && (!name || node.name === name)) result.push(node); if (Array.isArray(node.inner)) node.inner.forEach(walk); };
    walk(root); return result;
}

function probeClang(tool, tempRoot) {
    const versionResult = runTool(tool, ['--version'], tempRoot, tempRoot, 1024 * 1024);
    const version = Buffer.concat([versionResult.stdout, versionResult.stderr]);
    if (!version.length) block('clang_version_is_empty');
    const fixture = path.join(tempRoot, 'autoai-clang-capability-probe.cpp');
    fs.writeFileSync(fixture, 'int autoai_probe(int value) { return value + 1; }\n', { mode: 0o600 });
    const output = runTool(tool, ['-std=c++17', '-fsyntax-only', '-Xclang', '-ast-dump=json', fixture], tempRoot, tempRoot).stdout;
    const ast = parseJsonNoDuplicates(decode(output, 'clang_capability_ast'), 'clang_capability_ast');
    if (ast.kind !== 'TranslationUnitDecl') block('clang_capability_root_mismatch');
    const functions = findNodes(ast, 'FunctionDecl', 'autoai_probe');
    if (functions.length !== 1) block('clang_capability_function_mismatch');
    const fn = functions[0], parm = (fn.inner || []).find(x => x.kind === 'ParmVarDecl'), body = (fn.inner || []).find(x => x.kind === 'CompoundStmt');
    if (!fn.type || typeof fn.type.qualType !== 'string' || !parm?.type?.qualType || !body) block('clang_capability_fields_missing');
    const projection = {
        root_kind: ast.kind, declaration_kind: fn.kind, name: fn.name,
        function_type: normalizeType(fn.type.desugaredQualType || fn.type.qualType),
        parameter_type: normalizeType(parm.type.desugaredQualType || parm.type.qualType),
        body_kinds: [...new Set(findNodes(body).map(x => x.kind).filter(Boolean))].sort(cmp)
    };
    return { resolved_path: tool, version_sha256: sha(version), capability_probe_sha256: sha(Buffer.from(canonical(projection))) };
}

function readCompileDatabase(root, relativePath) {
    if (typeof relativePath !== 'string' || !relativePath.endsWith('.json')) invalid('compile_commands_path_must_be_json');
    const normalized = checkedRepoPath(root, root, relativePath, 'compile_commands_path');
    const file = path.join(root, ...normalized.split('/'));
    let st; try { st = fs.lstatSync(file); } catch { block('compile_commands_missing'); }
    if (!st.isFile() || st.isSymbolicLink() || st.size > MAX_DATABASE_BYTES) invalid('compile_commands_not_safe_regular_json');
    const bytes = fs.readFileSync(file), parsed = parseJsonNoDuplicates(decode(bytes, 'compile_commands', invalid), 'compile_commands', invalid);
    if (!Array.isArray(parsed) || !parsed.length || parsed.length > MAX_DATABASE_ENTRIES) invalid('compile_commands_entry_count_invalid');
    return { bytes, entries: parsed };
}

const COMPILERS = /^(?:clang\+\+|clang|g\+\+|gcc|c\+\+|cc)(?:-[0-9.]+)?$/;
const WRAPPERS = new Set(['ccache', 'sccache']);
const STRIP_ONE = new Set(['-c', '-pipe', '-pedantic', '-pedantic-errors', '-MD', '-MMD', '-MP']);
const STRIP_VALUE = new Set(['-o', '--output', '-MF', '-MT', '-MQ']);
const PAIR_VALUE = new Set(['-D', '-U', '-x', '-target']);
const PATH_VALUE = new Set(['-I', '-isystem', '-iquote', '-idirafter', '-include', '-imacros', '--sysroot', '-isysroot']);
const SAFE_EXACT = new Set(['-pthread', '-fPIC', '-fPIE', '-fno-exceptions', '-fexceptions', '-fno-rtti', '-frtti', '-nostdinc', '-nostdinc++', '-fchar8_t', '-fno-char8_t']);
const dangerous = value => value.startsWith('@') || /^(?:-Xclang|-load|-plugin|-fplugin|-fplugin-arg|-MJ|-serialize-diagnostics|-save-temps|-ftime-trace|-fprofile|-fcoverage|-coverage|-fmodules-cache-path)/.test(value);

function normalizeEntry(root, raw, index) {
    if (!raw || typeof raw !== 'object' || Array.isArray(raw)) invalid('compile_entry_not_object');
    const keys = Object.keys(raw), allowed = new Set(['directory', 'file', 'arguments', 'output']);
    if (keys.some(k => !allowed.has(k)) || keys.includes('command') || typeof raw.directory !== 'string' || typeof raw.file !== 'string' || !Array.isArray(raw.arguments) || !raw.arguments.length) invalid('compile_entry_schema_invalid');
    if (raw.arguments.length > MAX_ARGUMENTS || raw.arguments.some(x => typeof x !== 'string' || !x || Buffer.byteLength(x) > MAX_ARGUMENT_BYTES || x.includes('\0') || /[\r\n]/.test(x))) invalid('compile_entry_arguments_invalid');
    const directoryRel = checkedRepoPath(root, root, raw.directory, 'compile_directory');
    const directory = path.join(root, ...directoryRel.split('/'));
    let dirStat; try { dirStat = fs.lstatSync(directory); } catch { invalid('compile_directory_missing'); }
    if (!dirStat.isDirectory() || dirStat.isSymbolicLink()) invalid('compile_directory_not_safe');
    const fileRel = checkedRepoPath(root, directory, raw.file, 'compile_file');
    if (Object.prototype.hasOwnProperty.call(raw, 'output')) {
        if (typeof raw.output !== 'string' || !raw.output) invalid('compile_output_invalid');
        checkedRepoPath(root, directory, raw.output, 'compile_output');
    }
    let cursor = 0, first = path.basename(raw.arguments[cursor++]);
    if (WRAPPERS.has(first)) { if (raw.arguments.length < 2) invalid('compile_wrapper_missing_compiler'); first = path.basename(raw.arguments[cursor++]); }
    if (!COMPILERS.test(first)) invalid('unsupported_compile_driver');
    const flags = [], sources = [];
    const addPath = (flag, value) => { const rel = checkedRepoPath(root, directory, value, 'compile_argument_path'); flags.push({ kind: 'path', flag, path: rel }); };
    while (cursor < raw.arguments.length) {
        const arg = raw.arguments[cursor++];
        if (dangerous(arg)) invalid('dangerous_compile_argument');
        // Diagnostics/debug flags do not affect the AST projection.  Keep
        // optimization flags because they define macros such as __OPTIMIZE__.
        if (STRIP_ONE.has(arg) || /^-(?:g(?:\d+)?|W.+|fdiagnostics-.+)$/.test(arg)) continue;
        if (STRIP_VALUE.has(arg)) {
            if (cursor >= raw.arguments.length) invalid('compile_output_value_missing');
            const value = raw.arguments[cursor++];
            if (['-o', '--output', '-MF'].includes(arg)) checkedRepoPath(root, directory, value, 'compile_output_argument');
            continue;
        }
        if (PATH_VALUE.has(arg)) { if (cursor >= raw.arguments.length) invalid('compile_path_value_missing'); addPath(arg, raw.arguments[cursor++]); continue; }
        let matched = false;
        for (const flag of ['-isystem', '-iquote', '-idirafter', '-include', '-imacros', '-isysroot', '-I']) {
            if (arg.startsWith(flag) && arg.length > flag.length) { addPath(flag, arg.slice(flag.length)); matched = true; break; }
        }
        if (matched) continue;
        if (arg.startsWith('--sysroot=')) { addPath('--sysroot', arg.slice('--sysroot='.length)); continue; }
        if (PAIR_VALUE.has(arg)) {
            if (cursor >= raw.arguments.length) invalid('compile_flag_value_missing');
            const value = raw.arguments[cursor++]; if (!value || dangerous(value)) invalid('compile_flag_value_invalid');
            if (arg === '-x' && !['c++', 'c++-header'].includes(value)) invalid('compile_entry_is_not_cxx');
            flags.push({ kind: 'pair', flag: arg, value }); continue;
        }
        if (/^-[DU].+/.test(arg) || /^-std=[A-Za-z0-9+._-]+$/.test(arg) || /^--target=[A-Za-z0-9+._-]+$/.test(arg) || /^-O(?:[0-3sgz]|fast)?$/.test(arg) || /^-stdlib=(?:libc\+\+|libstdc\+\+)$/.test(arg) || SAFE_EXACT.has(arg)) { flags.push({ kind: 'plain', value: arg }); continue; }
        if (/^-m(?:arch|cpu|tune|abi|float-abi|fpu)=[A-Za-z0-9+._-]+$/.test(arg)) { flags.push({ kind: 'plain', value: arg }); continue; }
        if (arg.startsWith('-')) invalid('unsupported_compile_argument');
        const candidate = checkedRepoPath(root, directory, arg, 'compile_source_argument');
        if (!sourcePath(candidate)) invalid('compile_entry_is_not_cxx');
        sources.push(candidate);
    }
    if (sources.length !== 1 || sources[0] !== fileRel) invalid('compile_entry_source_mismatch');
    return { index, directory: directoryRel, file: fileRel, flags, key: fileRel + '\0' + directoryRel + '\0' + canonical(flags) };
}

function normalizeDatabase(root, rawEntries) {
    const byKey = new Map;
    rawEntries.forEach((raw, index) => { const entry = normalizeEntry(root, raw, index); if (!byKey.has(entry.key)) byKey.set(entry.key, entry); });
    return [...byKey.values()].sort((a, b) => cmp(a.key, b.key));
}

function materializeBase(root, implementationBase, target, workingDirectories = []) {
    if (typeof implementationBase !== 'string' || !/^[0-9a-f]{40,64}$/.test(implementationBase)) block('invalid_implementation_base');
    const tree = cp.spawnSync('git', ['ls-tree', '-rz', '--full-tree', implementationBase], { cwd: root, encoding: null, timeout: remainingTimeout('base_tree'), maxBuffer: 64 * 1024 * 1024, shell: false });
    if (tree.status !== 0 || tree.signal) block('base_tree_read_failed');
    const records = decode(tree.stdout, 'base_tree').split('\0').filter(Boolean), blobs = [];
    if (records.length > MAX_BASE_FILES) block('base_tree_file_limit_exceeded');
    for (const record of records) {
        const match = record.match(/^(\d+) (\S+) ([0-9a-f]{40,64})\t([\s\S]+)$/);
        if (!match) block('base_tree_record_invalid');
        const [, mode, type, object, relative] = match; safeRel(relative, 'base_tree_path');
        if (type !== 'blob' || !['100644', '100755'].includes(mode)) block('base_tree_contains_unsupported_entry');
        blobs.push({ mode, object, relative });
    }
    // Feed --batch from a bounded private regular file.  Besides making EOF
    // explicit, this avoids inheriting a writable pipe into the Git process.
    const requestFile = path.join(path.dirname(target), '.base-blob-batch-input');
    fs.writeFileSync(requestFile, blobs.map(x => x.object).join('\n') + (blobs.length ? '\n' : ''), { mode: 0o600 });
    const requestFd = fs.openSync(requestFile, 'r');
    let batch;
    try {
        batch = cp.spawnSync('git', ['cat-file', '--batch'], { cwd: root, stdio: [requestFd, 'pipe', 'pipe'], encoding: null, timeout: remainingTimeout('base_blobs'), maxBuffer: MAX_BASE_BYTES + 64 * 1024 * 1024, shell: false });
    } finally {
        fs.closeSync(requestFd); fs.unlinkSync(requestFile);
    }
    if (batch.status !== 0 || batch.signal) block('base_blob_batch_failed');
    let offset = 0, total = 0;
    for (const blob of blobs) {
        const newline = batch.stdout.indexOf(10, offset); if (newline < 0) block('base_blob_header_missing');
        const header = batch.stdout.subarray(offset, newline).toString('ascii'), match = header.match(/^([0-9a-f]{40,64}) blob (\d+)$/);
        if (!match || match[1] !== blob.object) block('base_blob_header_invalid');
        const size = Number(match[2]); if (!Number.isSafeInteger(size) || size < 0 || total + size > MAX_BASE_BYTES) block('base_blob_size_limit_exceeded');
        const begin = newline + 1, end = begin + size; if (end >= batch.stdout.length || batch.stdout[end] !== 10) block('base_blob_payload_invalid');
        const destination = path.join(target, ...blob.relative.split('/')); fs.mkdirSync(path.dirname(destination), { recursive: true, mode: 0o700 }); fs.writeFileSync(destination, batch.stdout.subarray(begin, end), { mode: 0o400 });
        offset = end + 1; total += size;
    }
    if (offset !== batch.stdout.length) block('base_blob_batch_trailing_data');
    // compile_commands.json commonly points at an untracked CMake build
    // directory. Recreate only normalized working directories inside the
    // private base mirror before making it read-only; never mutate the user's
    // checkout and reject a blob/directory collision.
    for (const relative of [...new Set(workingDirectories)].sort(cmp)) {
        safeRel(relative, 'base_compile_directory');
        const directory = relative === '.' ? target : path.join(target, ...relative.split('/'));
        try {
            if (fs.existsSync(directory)) {
                const st = fs.lstatSync(directory);
                if (!st.isDirectory() || st.isSymbolicLink()) invalid('base_compile_directory_collision');
            } else fs.mkdirSync(directory, { recursive: true, mode: 0o700 });
        } catch (error) {
            if (error?.gateStatus) throw error;
            if (['EEXIST', 'ENOTDIR'].includes(error?.code)) invalid('base_compile_directory_collision');
            block('base_compile_directory_create_failed');
        }
    }
    const directories = [];
    const walk = directory => { for (const name of fs.readdirSync(directory)) { const child = path.join(directory, name), st = fs.lstatSync(child); if (st.isSymbolicLink() || (!st.isDirectory() && !st.isFile())) block('base_mirror_contains_unsafe_entry'); if (st.isDirectory()) { walk(child); directories.push(child); } else fs.chmodSync(child, 0o400); } };
    walk(target); directories.sort((a, b) => b.length - a.length).forEach(directory => fs.chmodSync(directory, 0o500)); fs.chmodSync(target, 0o500);
}

function renderArgs(entry, sideRoot, sourceRel) {
    const result = [];
    for (const flag of entry.flags) {
        if (flag.kind === 'plain') result.push(flag.value);
        else if (flag.kind === 'pair') result.push(flag.flag, flag.value);
        else result.push(flag.flag, path.join(sideRoot, ...flag.path.split('/')));
    }
    result.push(path.join(sideRoot, ...sourceRel.split('/'))); return result;
}

function makeDependencies(text, sideRoot) {
    const flat = text.replace(/\\\r?\n/g, ' '), colon = flat.indexOf(':'); if (colon < 0) block('dependency_output_invalid');
    const tokens = []; let token = '', escaped = false;
    for (const char of flat.slice(colon + 1)) {
        if (escaped) { token += char; escaped = false; }
        else if (char === '\\') escaped = true;
        else if (/\s/.test(char)) { if (token) { tokens.push(token); token = ''; } }
        else token += char;
    }
    if (escaped) block('dependency_output_dangling_escape'); if (token) tokens.push(token);
    const result = new Set;
    for (const tokenValue of tokens) {
        const absolute = path.resolve(sideRoot, tokenValue); if (!isWithin(sideRoot, absolute)) continue;
        const relative = checkedRepoPath(sideRoot, sideRoot, absolute, 'dependency_path', true);
        result.add(relative);
    }
    return result;
}

function locationPath(node, inherited, sideRoot) {
    const raw = node?.loc?.file || node?.range?.begin?.file || inherited;
    if (!raw || typeof raw !== 'string' || raw.startsWith('<')) return inherited || null;
    const absolute = path.resolve(sideRoot, raw); if (!isWithin(sideRoot, absolute)) return null;
    const relative = path.relative(sideRoot, absolute).split(path.sep).join('/'); safeRel(relative, 'ast_declaration_path'); return relative;
}

const DECLARATION_KINDS = new Set(['FunctionDecl', 'CXXMethodDecl', 'CXXConstructorDecl', 'CXXDestructorDecl', 'CXXConversionDecl', 'CXXRecordDecl', 'RecordDecl']);
const UNSUPPORTED_PUBLIC_DECLARATIONS = new Set([
    'EnumDecl', 'FieldDecl', 'VarDecl', 'VarTemplateDecl', 'TypedefDecl', 'TypeAliasDecl', 'TypeAliasTemplateDecl',
    'UsingDecl', 'UsingShadowDecl', 'NamespaceAliasDecl', 'FriendDecl', 'ConceptDecl', 'CXXDeductionGuideDecl',
    'ClassTemplatePartialSpecializationDecl', 'ClassTemplateSpecializationDecl'
]);
const BODY_KINDS = new Set([
    'CompoundStmt', 'ReturnStmt', 'DeclStmt', 'VarDecl', 'ParmVarDecl', 'BinaryOperator', 'CompoundAssignOperator', 'UnaryOperator', 'ConditionalOperator',
    'CallExpr', 'CXXMemberCallExpr', 'CXXOperatorCallExpr', 'DeclRefExpr', 'MemberExpr', 'IntegerLiteral', 'FloatingLiteral', 'StringLiteral', 'CharacterLiteral',
    'CXXBoolLiteralExpr', 'CXXNullPtrLiteralExpr', 'GNUNullExpr', 'ImplicitCastExpr', 'CStyleCastExpr', 'CXXStaticCastExpr', 'CXXFunctionalCastExpr',
    'CXXConstructExpr', 'CXXTemporaryObjectExpr', 'MaterializeTemporaryExpr', 'ExprWithCleanups', 'ParenExpr', 'ArraySubscriptExpr', 'CXXThisExpr',
    'IfStmt', 'ForStmt', 'CXXForRangeStmt', 'WhileStmt', 'DoStmt', 'SwitchStmt', 'CaseStmt', 'DefaultStmt', 'BreakStmt', 'ContinueStmt', 'NullStmt',
    'InitListExpr', 'UnaryExprOrTypeTraitExpr', 'SubstNonTypeTemplateParmExpr', 'ConstantExpr'
]);
const nodeType = node => node?.type && normalizeType(node.type.desugaredQualType || node.type.qualType);
const templateKind = node => node.kind === 'TemplateTypeParmDecl' ? 'type' : node.kind === 'NonTypeTemplateParmDecl' ? 'non_type:' + nodeType(node) : node.kind === 'TemplateTemplateParmDecl' ? 'template' : null;

function functionReturn(node, declarationKind) {
    if (['constructor', 'destructor', 'type', 'class_template'].includes(declarationKind)) return null;
    const qual = nodeType(node), index = qual.indexOf('('); if (index <= 0) block('function_return_type_not_canonicalizable'); return normalizeType(qual.slice(0, index));
}

function collectDeclarations(ast, sideRoot, contractPathPolicy) {
    if (!ast || ast.kind !== 'TranslationUnitDecl') block('ast_root_mismatch');
    const raw = [];
    const nodeById = new Map, lexicalContextById = new Map, childContextById = new Map;
    const referenceId = value => typeof value === 'string' && value ? value : value && typeof value.id === 'string' && value.id ? value.id : null;
    const indexContexts = (node, context) => {
        if (!node || typeof node !== 'object') return;
        const id = referenceId(node.id);
        if (id) { nodeById.set(id, node); lexicalContextById.set(id, [...context]); }
        let childContext = context;
        if (node.kind === 'NamespaceDecl' && node.name) childContext = [...context, node.name];
        else if ((node.kind === 'CXXRecordDecl' || node.kind === 'RecordDecl') && node.name) childContext = [...context, node.name];
        if (id) childContextById.set(id, [...childContext]);
        for (const child of node.inner || []) indexContexts(child, childContext);
    };
    indexContexts(ast, []);
    const declarationContext = (node, fallback) => {
        let cursor = node; const seen = new Set;
        for (;;) {
            const cursorId = referenceId(cursor?.id);
            if (cursorId) {
                if (seen.has(cursorId)) block('ast_redeclaration_context_cycle');
                seen.add(cursorId);
            }
            const parentId = referenceId(cursor?.parentDeclContextId);
            if (parentId) {
                if (!childContextById.has(parentId)) block('ast_parent_decl_context_missing');
                return [...childContextById.get(parentId)];
            }
            const previousId = referenceId(cursor?.previousDecl);
            if (previousId) {
                if (!nodeById.has(previousId)) block('ast_previous_declaration_missing');
                cursor = nodeById.get(previousId);
                continue;
            }
            if (cursorId && lexicalContextById.has(cursorId)) return [...lexicalContextById.get(cursorId)];
            return [...fallback];
        }
    };
    const visit = (node, context, inheritedPath, inheritedAccess, anonymousNamespace, skipSelf = false, inheritedTemplateKinds = []) => {
        if (!node || typeof node !== 'object' || typeof node.kind !== 'string') block('ast_node_invalid');
        const declarationPath = locationPath(node, inheritedPath, sideRoot), implicit = node.isImplicit === true;
        if (!implicit && declarationPath && contractPathPolicy(declarationPath) &&
            (UNSUPPORTED_PUBLIC_DECLARATIONS.has(node.kind) || /Attr$/.test(node.kind) || /(?:Requires|Requirement|ConceptSpecialization|TypeConstraint)/.test(node.kind))) {
            block('unsupported_public_contract_declaration_' + node.kind);
        }
        const semanticContext = (DECLARATION_KINDS.has(node.kind) || node.kind === 'FunctionTemplateDecl' || node.kind === 'ClassTemplateDecl') ? declarationContext(node, context) : context;
        if (node.kind === 'NamespaceDecl') {
            const anonymous = !node.name; const next = anonymous ? [...semanticContext, '(anonymous namespace)'] : [...semanticContext, node.name];
            for (const child of node.inner || []) visit(child, next, declarationPath, 'none', anonymousNamespace || anonymous, false, inheritedTemplateKinds); return;
        }
        if (node.kind === 'FunctionTemplateDecl' || node.kind === 'ClassTemplateDecl') {
            if (implicit) return;
            const primaryKind = node.kind === 'FunctionTemplateDecl' ? new Set(['FunctionDecl', 'CXXMethodDecl']) : new Set(['CXXRecordDecl', 'RecordDecl']);
            const primary = (node.inner || []).find(child => primaryKind.has(child.kind) && !child.isImplicit);
            if (!primary) block('template_primary_declaration_missing');
            const ownTemplateKinds = (node.inner || []).map(templateKind).filter(Boolean), templateKinds = [...inheritedTemplateKinds, ...ownTemplateKinds];
            for (const parameter of node.inner || []) if (templateKind(parameter) && (parameter.defaultArg || parameter.hasDefaultArg)) block('unsupported_template_default_argument');
            addDeclaration(primary, node.kind === 'FunctionTemplateDecl' ? 'function_template' : 'class_template', declarationContext(primary, semanticContext), declarationPath, inheritedAccess, anonymousNamespace, templateKinds, node);
            // The class-template primary owns the methods.  Skipping this
            // traversal used to make every template member invisible.
            if (node.kind === 'ClassTemplateDecl') visit(primary, semanticContext, declarationPath, inheritedAccess, anonymousNamespace, true, templateKinds);
            for (const child of node.inner || []) if (child !== primary && !templateKind(child)) visit(child, semanticContext, declarationPath, inheritedAccess, anonymousNamespace, false, inheritedTemplateKinds); return;
        }
        if (!skipSelf && DECLARATION_KINDS.has(node.kind) && !implicit) addDeclaration(node, null, semanticContext, declarationPath, inheritedAccess, anonymousNamespace, inheritedTemplateKinds, node);
        if (node.kind === 'CXXRecordDecl' || node.kind === 'RecordDecl') {
            const recordName = node.name; if (!recordName) { if (!implicit) block('anonymous_record_not_supported'); return; }
            let access = node.tagUsed === 'class' ? 'private' : 'public'; const next = [...semanticContext, recordName];
            for (const child of node.inner || []) { if (child.kind === 'AccessSpecDecl') { if (!['public', 'protected', 'private'].includes(child.access)) block('record_access_invalid'); access = child.access; } else visit(child, next, declarationPath, access, anonymousNamespace, false, inheritedTemplateKinds); }
            return;
        }
        for (const child of node.inner || []) if (!BODY_KINDS.has(child.kind)) visit(child, semanticContext, declarationPath, inheritedAccess, anonymousNamespace, false, inheritedTemplateKinds);
    };
    const addDeclaration = (node, forcedKind, context, inheritedPath, inheritedAccess, anonymousNamespace, templateKinds, semanticNode) => {
        const declarationPath = locationPath(node, inheritedPath, sideRoot); if (!declarationPath || !fileExists(path.join(sideRoot, ...declarationPath.split('/')))) return;
        let kind = forcedKind;
        if (!kind) kind = node.kind === 'FunctionDecl' ? 'function' : node.kind === 'CXXMethodDecl' ? (String(node.name).startsWith('operator') ? 'operator' : 'method') : node.kind === 'CXXConstructorDecl' ? 'constructor' : node.kind === 'CXXDestructorDecl' ? 'destructor' : node.kind === 'CXXConversionDecl' ? 'conversion' : 'type';
        if (typeof node.name !== 'string' || !node.name) block('declaration_name_missing');
        const parameterNodes = (node.inner || []).filter(x => x.kind === 'ParmVarDecl');
        if (parameterNodes.some(parameter => parameter.init || parameter.hasDefaultArg || (parameter.inner || []).length)) block('unsupported_default_argument');
        const parameters = parameterNodes.map(nodeType);
        const type = ['type', 'class_template'].includes(kind) ? null : nodeType(node), suffix = type ? type.slice(type.lastIndexOf(')') + 1) : '';
        const identityCore = {
            declaration_kind: kind, qualified_name: [...context, node.name].join('::'), canonical_parameter_types: parameters,
            canonical_return_type: functionReturn(node, kind), template_parameter_kinds: templateKinds,
            cv_qualifiers: ['const', 'volatile'].filter(q => new RegExp('(?:^|\\s)' + q + '(?:\\s|$)').test(suffix)),
            ref_qualifier: /&&/.test(suffix) ? 'rvalue' : /(?:^|\s)&(?:\s|$)/.test(suffix) ? 'lvalue' : 'none'
        };
        const body = (node.inner || []).find(x => BODY_KINDS.has(x.kind) && !['ParmVarDecl'].includes(x.kind)) || null;
        const access = ['public', 'protected', 'private'].includes(node.access) ? node.access : inheritedAccess || 'none';
        const freeStatic = node.kind === 'FunctionDecl' && node.storageClass === 'static';
        const linkage = freeStatic || anonymousNamespace ? 'internal' : node.moduleOwnershipKind ? 'module' : 'external';
        const bases = (node.bases || []).map(base => {
            if (!base?.type) block('unsupported_record_base_projection');
            return { type: normalizeType(base.type.desugaredQualType || base.type.qualType), access: base.access || base.writtenAccess || 'none', virtual: base.isVirtual === true };
        });
        const contract = {
            type, access, linkage, virtual: node.virtual === true, pure: node.pure === true, static: node.storageClass === 'static', constexpr: node.constexpr === true,
            consteval: node.consteval === true, deleted: node.explicitlyDeleted === true, defaulted: node.explicitlyDefaulted === true,
            noexcept: typeof type === 'string' && /\bnoexcept\b/.test(type), bases
        };
        raw.push({ node, semanticNode, declarationPath, identityCore, access, linkage, contract, body, definition: !!body, nodeId: node.id || null });
    };
    for (const child of ast.inner || []) visit(child, [], locationPath(child, null, sideRoot), 'none', false);
    const groups = new Map;
    for (const item of raw) { const key = canonical(item.identityCore); if (!groups.has(key)) groups.set(key, []); groups.get(key).push(item); }
    const idToIdentity = new Map, prepared = [];
    for (const [coreKey, items] of groups) {
        const contractPaths = items.map(x => x.declarationPath).filter(contractPathPolicy).sort(cmp);
        const headerPaths = items.map(x => x.declarationPath).filter(headerPath).sort(cmp);
        const declarationPath = contractPaths[0] || headerPaths[0] || items.map(x => x.declarationPath).sort(cmp)[0];
        const identity = { ...items[0].identityCore, declaration_path: declarationPath };
        const accesses = [...new Set(items.map(x => x.access).filter(x => x !== 'none'))], linkages = [...new Set(items.map(x => x.linkage).filter(x => x !== 'unknown'))];
        if (accesses.length > 1 || linkages.length > 1) block('redeclaration_contract_conflict');
        const access = accesses[0] || 'none', linkage = linkages[0] || 'unknown';
        const contracts = items.map(x => canonical({ ...x.contract, access, linkage })); if (new Set(contracts).size !== 1) block('redeclaration_contract_projection_conflict');
        const definitions = items.filter(x => x.definition); if (definitions.length > 1) block('multiple_definitions_in_ast_entry');
        items.forEach(x => { if (x.nodeId) idToIdentity.set(x.nodeId, identity); });
        prepared.push({ coreKey, identity, access, linkage, contract: { ...items[0].contract, access, linkage }, definition: definitions[0] || null });
    }
    const projectBody = node => {
        if (!node || typeof node.kind !== 'string' || !BODY_KINDS.has(node.kind)) block('unsupported_semantic_ast_node');
        const out = { kind: node.kind };
        if (node.type) out.type = nodeType(node);
        for (const key of ['opcode', 'value', 'valueCategory', 'castKind', 'isArrow', 'name']) if (['string', 'number', 'boolean'].includes(typeof node[key])) out[key] = node[key];
        if (node.referencedDecl) {
            const referenced = node.referencedDecl.id && idToIdentity.get(node.referencedDecl.id);
            out.referenced_identity = referenced || { declaration_kind: node.referencedDecl.kind || null, name: node.referencedDecl.name || null, type: node.referencedDecl.type ? normalizeType(node.referencedDecl.type.desugaredQualType || node.referencedDecl.type.qualType) : null };
        }
        if (Array.isArray(node.inner)) {
            if (node.inner.some(x => !BODY_KINDS.has(x.kind))) block('unsupported_semantic_ast_node');
            out.inner = node.inner.map(projectBody);
        }
        return out;
    };
    const result = new Map;
    for (const item of prepared) {
        const semantic = { identity: item.identity, contract: item.contract, body: item.definition ? projectBody(item.definition.body) : null };
        const key = canonical(item.identity); if (result.has(key)) invalid('normalized_identity_collision');
        result.set(key, { identity: item.identity, access: item.access, linkage: item.linkage, semantic_sha256: sha(Buffer.from(canonical(semantic))) });
    }
    return result;
}

function analyzeInvocation(tool, entry, sideRoot, sourceRel, tempRoot, contractPathPolicy) {
    const baseArgs = renderArgs(entry, sideRoot, sourceRel), source = baseArgs.at(-1), flags = baseArgs.slice(0, -1);
    if (!fileExists(source)) block('translation_unit_not_regular_file');
    const compileDirectory = path.join(sideRoot, ...entry.directory.split('/'));
    let directoryStat; try { directoryStat = fs.lstatSync(compileDirectory); } catch { block('mapped_compile_directory_missing'); }
    if (!directoryStat.isDirectory() || directoryStat.isSymbolicLink()) block('mapped_compile_directory_not_safe');
    const dependency = runTool(tool, [...flags, '-M', source], compileDirectory, tempRoot, MAX_AST_BYTES);
    const dependencies = makeDependencies(decode(dependency.stdout, 'clang_dependencies'), sideRoot);
    const astResult = runTool(tool, [...flags, '-fsyntax-only', '-Xclang', '-ast-dump=json', source], compileDirectory, tempRoot, MAX_AST_BYTES);
    const ast = parseJsonNoDuplicates(decode(astResult.stdout, 'clang_ast'), 'clang_ast');
    return { dependencies, declarations: collectDeclarations(ast, sideRoot, contractPathPolicy) };
}

function sameProjection(left, right) {
    if (left.size !== right.size) return false;
    for (const [key, value] of left) if (!right.has(key) || canonical(value) !== canonical(right.get(key))) return false;
    return true;
}

function identityInventory(aggregate) {
    const collect = side => [...aggregate[side].values()].map(value => value.identity).sort((a, b) => cmp(canonical(a), canonical(b)));
    return { base: collect('base'), current: collect('current') };
}

function validatePlannedIdentityInventory(plan, inventory) {
    if (!inventory || !Array.isArray(inventory.base) || !Array.isArray(inventory.current)) invalid('ast_identity_inventory_invalid');
    const sets = { base: new Set(inventory.base.map(canonical)), current: new Set(inventory.current.map(canonical)) }, owners = { base: new Map, current: new Map };
    if (sets.base.size !== inventory.base.length || sets.current.size !== inventory.current.length) invalid('ast_identity_inventory_collision');
    for (const surface of plan.block.surfaces || []) {
        if (surface.symbol_identities === null || surface.symbol_identities === undefined) continue;
        for (const side of ['base', 'current']) for (const identity of surface.symbol_identities[side] || []) owners[side].set(canonical(identity), surface);
    }
    for (const surface of plan.block.surfaces || []) {
        if (surface.symbol_identities === null) continue;
        if (!surface.symbol_identities || !Array.isArray(surface.symbol_identities.base) || !Array.isArray(surface.symbol_identities.current) || !Array.isArray(surface.producer_paths)) invalid('planned_symbol_identity_contract_invalid');
        for (const side of ['base', 'current']) for (const identity of surface.symbol_identities[side]) {
            if (!surface.producer_paths.includes(identity.declaration_path)) invalid('planned_symbol_declaration_is_not_a_producer:' + (surface.id || 'unknown'));
            if (!sets[side].has(canonical(identity))) invalid('planned_symbol_identity_missing_from_' + side + ':' + (surface.id || 'unknown'));
        }
        if (surface.change_kind === 'added' && surface.symbol_identities.current.some(identity => {
            const key = canonical(identity), priorOwner = owners.base.get(key);
            return sets.base.has(key) && (!priorOwner || priorOwner.change_kind !== 'removed');
        })) invalid('added_symbol_already_exists_in_base:' + surface.id);
        if (surface.change_kind === 'removed' && surface.symbol_identities.base.some(identity => {
            const key = canonical(identity), nextOwner = owners.current.get(key);
            return sets.current.has(key) && (!nextOwner || nextOwner.change_kind !== 'added');
        })) invalid('removed_symbol_still_exists_in_current:' + surface.id);
        if (['modified', 'deprecated'].includes(surface.change_kind) && (!surface.symbol_identities.base.length || !surface.symbol_identities.current.length)) invalid('changed_symbol_requires_both_tree_sides:' + surface.id);
    }
}

function discover(options) {
    const root = path.resolve(options?.root || '.');
    if (!options || !options.plan?.block || !Array.isArray(options.plan.block.surfaces) || !Array.isArray(options.scope?.logical_changes) || !options.classification) invalid('ast_adapter_options_invalid');
    let contractPathPolicy;
    try { contractPathPolicy = scopeLib.compileContractPathPolicy(options.classification); }
    catch (error) { invalid('ast_contract_path_policy_invalid:' + error.message); }
    if (options.plan.block.surfaces.some(surface => ['internal_api', 'external_api'].includes(surface.kind) && (surface.symbol_identities === null || surface.symbol_identities === undefined))) invalid('clang_ast_cpp_api_requires_symbol_identities');
    discoveryDeadline = Date.now() + TOTAL_DISCOVERY_TIMEOUT_MS;
    let rootStat; try { rootStat = fs.lstatSync(root); } catch { block('repository_root_missing'); }
    if (!rootStat.isDirectory() || rootStat.isSymbolicLink()) block('repository_root_unsafe');
    const compile = readCompileDatabase(root, options.compileCommandsPath), entries = normalizeDatabase(root, compile.entries);
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'autoai-clang-ast-')); fs.chmodSync(tempRoot, 0o700);
    const baseRoot = path.join(tempRoot, 'base'); fs.mkdirSync(baseRoot, { mode: 0o700 });
    try {
        const tool = resolveClang(), toolIdentity = probeClang(tool, tempRoot);
        materializeBase(root, options.implementationBase, baseRoot, entries.map(entry => entry.directory));
        const renameNewToOld = new Map, targets = [];
        for (const change of options.scope.logical_changes) {
            safeRel(change.path, 'changed_path'); if (change.old_path) safeRel(change.old_path, 'changed_old_path');
            if (change.change_status === 'renamed') { if (!change.old_path || renameNewToOld.has(change.path)) block('translation_unit_rename_is_not_unique'); renameNewToOld.set(change.path, change.old_path); }
            const pair = change.change_status === 'renamed' ? { base: change.old_path, current: change.path } : change.change_status === 'added' ? { base: null, current: change.path } : change.change_status === 'deleted' ? { base: change.path, current: null } : { base: change.path, current: change.path };
            if ([pair.base, pair.current].some(x => x && (headerPath(x) || sourcePath(x)))) targets.push(pair);
        }
        for (const surface of options.plan.block.surfaces || []) {
            if (!surface.symbol_identities) continue;
            for (const identity of surface.symbol_identities.base || []) targets.push({ base: identity.declaration_path, current: null });
            for (const identity of surface.symbol_identities.current || []) targets.push({ base: null, current: identity.declaration_path });
        }
        const uniqueTargets = [...new Map(targets.map(x => [canonical(x), x])).values()];
        const hasHeaderTarget = uniqueTargets.some(x => (x.base && headerPath(x.base)) || (x.current && headerPath(x.current)));
        const sideEntries = { base: [], current: [] };
        for (const entry of entries) {
            if (fileExists(path.join(root, ...entry.file.split('/')))) sideEntries.current.push({ entry, source: entry.file });
            const baseSame = fileExists(path.join(baseRoot, ...entry.file.split('/'))), renamed = renameNewToOld.get(entry.file);
            if (baseSame) sideEntries.base.push({ entry, source: entry.file });
            else if (renamed && fileExists(path.join(baseRoot, ...renamed.split('/')))) sideEntries.base.push({ entry, source: renamed });
        }
        const analysis = { base: new Map, current: new Map }; let invocationCount = 0;
        const getAnalysis = (side, invocation) => {
            const key = invocation.entry.key + '\0' + invocation.source;
            if (!analysis[side].has(key)) {
                invocationCount++; if (invocationCount > MAX_AST_INVOCATIONS) block('ast_invocation_limit_exceeded');
                analysis[side].set(key, analyzeInvocation(tool, invocation.entry, side === 'base' ? baseRoot : root, invocation.source, tempRoot, contractPathPolicy));
            }
            return analysis[side].get(key);
        };
        const aggregate = { base: new Map, current: new Map };
        const merge = (side, projection) => { for (const [key, value] of projection) { if (aggregate[side].has(key) && canonical(aggregate[side].get(key)) !== canonical(value)) block('ast_cohort_projection_conflict'); aggregate[side].set(key, value); } };
        for (const side of ['base', 'current']) for (const target of uniqueTargets.map(x => x[side]).filter(Boolean)) {
            const sideRoot = side === 'base' ? baseRoot : root, targetFile = path.join(sideRoot, ...target.split('/'));
            if (!fileExists(targetFile)) block('required_ast_target_missing');
            let cohort;
            if (headerPath(target)) {
                cohort = sideEntries[side].filter(invocation => getAnalysis(side, invocation).dependencies.has(target));
                if (!cohort.length) block('changed_header_has_empty_dependency_cohort');
            } else {
                cohort = sideEntries[side].filter(invocation => invocation.source === target);
                if (!cohort.length) block('changed_translation_unit_has_no_compile_entry');
            }
            const projections = cohort.map(invocation => {
                const all = getAnalysis(side, invocation).declarations;
                if (!headerPath(target)) return all;
                return new Map([...all].filter(([, value]) => value.identity.declaration_path === target));
            });
            if (!projections.length || projections.slice(1).some(value => !sameProjection(projections[0], value))) block('ast_multi_configuration_inconsistency');
            merge(side, projections[0]);
        }
        // A header change requires every normalized entry that exists on a side
        // to be dependency-probed, even when it does not join that header cohort.
        if (hasHeaderTarget) for (const side of ['base', 'current']) for (const invocation of sideEntries[side]) getAnalysis(side, invocation);
        const candidates = [], keys = new Set([...aggregate.base.keys(), ...aggregate.current.keys()]);
        for (const key of [...keys].sort(cmp)) {
            const before = aggregate.base.get(key), after = aggregate.current.get(key);
            let status;
            if (before && after) { if (canonical(before.identity) !== canonical(after.identity)) invalid('ast_identity_pairing_error'); if (before.semantic_sha256 === after.semantic_sha256 && before.access === after.access && before.linkage === after.linkage) continue; status = 'modified'; }
            else status = after ? 'added' : 'deleted';
            const baseIdentity = before?.identity || null, currentIdentity = after?.identity || null;
            const digest = crypto.createHash('sha256').update(ADAPTER_ID + status + canonical(baseIdentity) + canonical(currentIdentity)).digest('hex').slice(0, 16);
            const sideValues = [before, after].filter(Boolean), publicContract = sideValues.some(value => value.access !== 'private' && value.linkage !== 'internal' && contractPathPolicy(value.identity.declaration_path));
            candidates.push({
                candidate_id: 'clang-ast-' + digest, source: 'clang_ast', base_symbol_identity: baseIdentity, current_symbol_identity: currentIdentity,
                candidate_scope: publicContract ? 'public_contract' : 'reviewable', base_access: before?.access || null, current_access: after?.access || null,
                base_linkage: before?.linkage || null, current_linkage: after?.linkage || null, change_status: status,
                base_semantic_sha256: before?.semantic_sha256 || null, current_semantic_sha256: after?.semantic_sha256 || null
            });
        }
        const ids = candidates.map(x => x.candidate_id); if (ids.length !== new Set(ids).size) invalid('ast_candidate_id_collision');
        candidates.sort((a, b) => cmp(a.candidate_id, b.candidate_id));
        const inventory = identityInventory(aggregate); validatePlannedIdentityInventory(options.plan, inventory);
        const adapterFile = fs.realpathSync(__filename);
        return {
            candidates, compile_commands_sha256: sha(compile.bytes), ast_tool_identity: toolIdentity,
            adapter_identity: { id: ADAPTER_ID, schema_version: ADAPTER_SCHEMA, sha256: sha(fs.readFileSync(adapterFile)) },
            identity_inventory: inventory
        };
    } finally {
        try {
            const relax = directory => { if (!fs.existsSync(directory)) return; const st = fs.lstatSync(directory); if (st.isDirectory() && !st.isSymbolicLink()) { fs.chmodSync(directory, 0o700); for (const name of fs.readdirSync(directory)) relax(path.join(directory, name)); } else if (!st.isSymbolicLink()) fs.chmodSync(directory, 0o600); };
            relax(tempRoot); fs.rmSync(tempRoot, { recursive: true, force: true });
        } catch { block('ast_temporary_cleanup_failed'); }
        finally { discoveryDeadline = null; }
    }
}

module.exports = { discover };
