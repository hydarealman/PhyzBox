# Contributing to PhyzBox

Thanks for your interest in contributing! PhyzBox is an open-source 3D astrophysics sandbox, and all contributions — bug reports, feature suggestions, code improvements, documentation — are welcome.

## Ways to Contribute

### 🐛 Report Bugs

Open an [issue](../../issues) with:

- A clear title and description
- Steps to reproduce (what scene, what you pressed, what happened)
- Your system info (Windows version, GPU, display resolution)
- If possible, attach a screenshot or screen recording

### 💡 Suggest Features

Open a [feature request](../../issues/new?template=feature_request.md) describing:

- What you'd like to see added
- Why it would be useful
- Any implementation ideas (optional)

### 🛠 Submit Code

1. Fork the repository
2. Create a branch: `git checkout -b feature/my-feature` or `fix/issue-42`
3. Make your changes
4. Ensure it still builds: `powershell -ExecutionPolicy Bypass -File .\build.ps1`
5. Run the self-test: `.\bin\PhyzBox.exe --self-test`
6. Commit with clear messages
7. Push and open a Pull Request

### 📖 Improve Documentation

Typos, better explanations, English translations, or additional preset scenarios — all are appreciated.

## Development Setup

- **Compiler**: MinGW-w64 (g++) with C++20 support
- **Build**: `build.bat` or `build.ps1` from project root
- **Dependencies**: None — only Windows system libraries (`opengl32`, `gdi32`, `user32`)
- **Debug**: Build with `-g -O0` for debugging; the `--self-test` flag runs headless physics validation

## Code Style

- C++20 with RAII and value semantics where practical
- 4-space indentation, no tabs
- `PascalCase` for class names, `camelCase` for methods and variables
- Comments in Chinese or English — whichever you're comfortable with
- Keep dependencies to zero: no external libraries

## Pull Request Checklist

- [ ] Code compiles without warnings
- [ ] `--self-test` passes
- [ ] If adding a feature, consider adding a new preset scenario or updating an existing one
- [ ] Update `DEVELOPMENT_LOG.md` with notes if adding significant new physics or rendering

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
