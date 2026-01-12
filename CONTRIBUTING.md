# Contributing to Nexus

Thank you for your interest in contributing to Nexus! This document provides guidelines for contributing.

## Code of Conduct

Please be respectful and constructive in all interactions.

## How to Contribute

### Reporting Bugs

1. Check existing issues to avoid duplicates
2. Use the bug report template
3. Include:
   - Platform and version
   - Steps to reproduce
   - Expected vs actual behavior
   - Relevant logs or screenshots

### Suggesting Features

1. Check existing feature requests
2. Use the feature request template
3. Describe the use case and benefits

### Pull Requests

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Ensure tests pass: `ctest --test-dir build`
5. Follow code style guidelines
6. Commit with conventional commits: `feat(hal): add PWM support`
7. Push and create a Pull Request

## Code Style

### C Code

- Follow `.clang-format` configuration
- Use Doxygen comments with backslash style (`\brief`, `\param`)
- Maximum line length: 80 characters
- Indent: 4 spaces (no tabs)

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Files | snake_case | `hal_gpio.c` |
| Functions | snake_case | `hal_gpio_init()` |
| Types | snake_case_t | `hal_gpio_config_t` |
| Macros | UPPER_CASE | `HAL_GPIO_PORT_MAX` |
| Enums | UPPER_CASE | `HAL_GPIO_DIR_INPUT` |

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`

## Testing

- Write unit tests for new features
- Maintain ≥90% code coverage
- Run tests before submitting PR

## Documentation

- Update API documentation for public interfaces
- Add examples for new features
- Keep README up to date

## Review Process

1. Automated CI checks must pass
2. At least one maintainer approval required
3. Address all review comments
4. Squash commits if requested

## Questions?

Open a discussion or reach out to maintainers.

Thank you for contributing! 🎉
