# Documentation Templates

This directory contains templates for creating new documentation pages in the Nexus project.

## Available Templates

### module_template.rst

Template for documenting a new module (HAL, OSAL, framework component).

**Usage:**
```bash
cp _templates/module_template.rst user_guide/my_module.rst
# Edit my_module.rst with your module's information
```

**Sections included:**
- Overview and key features
- Architecture with diagrams
- API reference with Doxygen integration
- Usage examples
- Configuration options
- Error handling
- Thread safety
- Performance considerations
- Troubleshooting

### tutorial_template.rst

Template for creating step-by-step tutorials.

**Usage:**
```bash
cp _templates/tutorial_template.rst tutorials/my_tutorial.rst
# Edit my_tutorial.rst with your tutorial content
```

**Sections included:**
- Prerequisites and learning objectives
- Step-by-step instructions
- Code examples with explanations
- Testing procedures
- Enhancements and variations
- Troubleshooting
- Complete working example

### platform_guide_template.rst

Template for platform-specific documentation.

**Usage:**
```bash
cp _templates/platform_guide_template.rst platform_guides/my_platform.rst
# Edit my_platform.rst with platform details
```

**Sections included:**
- Platform overview and specifications
- Supported peripherals
- Hardware setup and connections
- Toolchain installation
- Build configuration
- Flashing and debugging
- Platform-specific examples
- Limitations and known issues
- Performance benchmarks

## Template Guidelines

### General Guidelines

1. **Replace placeholder text**: All text in `[brackets]` or marked with `..` should be replaced with actual content
2. **Remove unused sections**: Delete sections that don't apply to your documentation
3. **Add sections as needed**: Feel free to add additional sections relevant to your topic
4. **Follow style guide**: Maintain consistent formatting and terminology
5. **Include examples**: Always provide working code examples
6. **Add cross-references**: Link to related documentation using `:doc:` directive

### RST Formatting

- **Headings**: Use consistent underline characters (=, -, ^, ", ~)
- **Code blocks**: Always specify language (c, python, bash, etc.)
- **Lists**: Use `-` for bullet lists, `1.` for numbered lists
- **Tables**: Use `list-table` directive for better control
- **Links**: Use `:doc:` for internal links, standard RST for external

### Code Examples

- Keep examples minimal and focused
- Include comments explaining key points
- Show both basic and advanced usage
- Ensure examples compile and run
- Use realistic variable names

### Diagrams

- Use Mermaid for flowcharts and diagrams
- Include alt text for accessibility
- Keep diagrams simple and clear
- Use consistent styling

## Adding Templates to Index

After creating a new page from a template, add it to the appropriate `index.rst` or `toctree`:

```rst
.. toctree::
   :maxdepth: 2
   
   existing_page
   my_new_page
```

## Questions?

If you have questions about using these templates, please refer to:
- :doc:`../development/contributing` - Contribution guidelines
- :doc:`../development/documentation_guide` - Documentation style guide
