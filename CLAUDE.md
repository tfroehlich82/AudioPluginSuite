# CLAUDE.md

The project knowledge base is **[AGENTS.md](AGENTS.md)**. Read it first. It covers the layout, every parameter, the engine invariants, the test setup, the decisions and their reasons, the open risks, and the JUCE and CMake gotchas.

This file holds only Claude Code specifics.

## Claude Code notes

- **Build tooling.** CMake is not on the bash PATH. Use the full path:
  `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
  (and `ctest.exe` in the same folder). See AGENTS.md section 3.
- **Files change on disk.** The IDE and the owner edit files between reads. The harness reports changes; take the on-disk state as current and re-read before editing.
- **Git.** Do not commit or push unless the owner asks.
- **Test expectations.** Run the full CTest suite before reporting a change as working. Editor tests need a desktop session, so a headless run can fail for reasons that are not code bugs.
- **Prefer the existing helpers.** `tests/common/PluginTestUtils.h` and the DSP tests already cover most patterns. Reuse them before writing new ones.
