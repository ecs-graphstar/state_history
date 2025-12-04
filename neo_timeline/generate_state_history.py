#!/usr/bin/env python3
"""
Generate StateHistory header from Jinja2 template.

This script automatically discovers component structs from components.h using
libclang and generates the state_history.h header file with all the boilerplate
code for component tracking.
"""

import os
import sys
from jinja2 import Environment, FileSystemLoader
from clang.cindex import Index, CursorKind


def extract_components_from_header(header_path='include/components.h'):
    """
    Parse components.h using libclang and extract all struct definitions.

    Returns:
        List of component dictionaries with 'name' field
    """
    if not os.path.exists(header_path):
        print(f"Error: {header_path} not found!")
        sys.exit(1)

    # Initialize libclang index
    index = Index.create()

    # Parse the header file
    translation_unit = index.parse(
        header_path,
        args=['-x', 'c++', '-std=c++17']
    )

    # Check for parsing errors
    if translation_unit.diagnostics:
        for diag in translation_unit.diagnostics:
            if diag.severity >= 3:  # Error or Fatal
                print(f"Parse error: {diag.spelling}")
                sys.exit(1)

    components = []

    def visit_node(node):
        """Recursively visit AST nodes to find struct declarations."""
        # Check if this is a struct definition in the target file
        if (node.kind == CursorKind.STRUCT_DECL and
            node.is_definition() and
            node.location.file and
            os.path.samefile(node.location.file.name, header_path)):

            struct_name = node.spelling
            if struct_name:  # Skip anonymous structs
                components.append({'name': struct_name})
                print(f"  Found component: {struct_name}")

        # Recursively visit children
        for child in node.get_children():
            visit_node(child)

    # Start traversing from the root
    visit_node(translation_unit.cursor)

    return components


def generate_header(template_dir='templates', output_dir='include', components_header='include/components.h'):
    """Generate the state_history.h header from the template."""

    print("Parsing components from", components_header)
    components = extract_components_from_header(components_header)

    if not components:
        print("Warning: No components found!")
        return

    # Setup Jinja2 environment
    env = Environment(
        loader=FileSystemLoader(template_dir),
        trim_blocks=True,
        lstrip_blocks=True,
    )

    # Load the template
    template = env.get_template('state_history.h.j2')

    # Render the template with component configuration
    rendered = template.render(components=components)

    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Write the generated header
    output_path = os.path.join(output_dir, 'state_history.h')
    with open(output_path, 'w') as f:
        f.write(rendered)

    print(f"\nGenerated {output_path}")
    print(f"Registered {len(components)} components:")
    for comp in components:
        print(f"  - {comp['name']}")


if __name__ == '__main__':
    generate_header()
