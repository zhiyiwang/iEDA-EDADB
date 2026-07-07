# iEDA-EDADB Personal Documentation Index

This directory keeps personal learning notes, research notes, and environment notes outside the iEDA source-tree documentation.

## Recommended Reading Order

1. `ieda_architecture_learning/00_learning_map.md` - iEDA architecture learning map and navigation.
2. `ieda_architecture_learning/10_complete_tutorial.md` - complete iEDA architecture and physical-design flow tutorial.
3. `../src/database/edadb/docs/README.md` - canonical EDADB adapter documentation index.
4. `paper/README.md` - EDADB + iEDA research roadmap and paper-oriented notes.
5. `codex_vscode_understand_anything_setup.md` - Codex / VS Code / Understand-Anything setup notes.

## Directory Roles

- `ieda_architecture_learning/`: code architecture, runtime flow, Tcl dispatch, iDB object map, and EDADB integration notes.
- `paper/`: research ideas, literature notes, tool opportunity matrix, and execution plans.
- `../src/database/edadb/docs/`: canonical EDADB adapter documentation tied to the source code.

## GitHub Sync

To clone only documentation on another machine:

```bash
git clone --filter=blob:none --no-checkout git@github.com:<user>/<repo>.git
cd <repo>
git sparse-checkout init --cone
git sparse-checkout set md src/database/edadb/docs
git checkout edadb-idb
```

Use normal `git pull` afterward to sync documentation updates.
