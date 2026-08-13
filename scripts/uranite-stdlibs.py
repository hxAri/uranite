#!/usr/bin/env python3

"""
Uranite Standard Library Entity Tree Generator

Parses all .urn files under stdlibs/ and generates a hierarchical tree
showing every entity: packages, classes, interfaces, structs, enums,
functions, methods, fields, constants, and enum variants.

Usage:
    python3 scripts/stdlibs-tree.py [--output FILE] [--format text|json|markdown]
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional


class EntityKind(Enum):
    PACKAGE = "package"
    CLASS = "class"
    ABSTRACT_CLASS = "abstract class"
    FINAL_CLASS = "final class"
    READONLY_CLASS = "readonly class"
    INTERFACE = "interface"
    STRUCT = "struct"
    ENUM = "enum"
    FUNCTION = "function"
    METHOD = "method"
    PROPERTY = "property"
    FIELD = "field"
    CONSTANT = "const"
    ENUM_VARIANT = "unit"
    EXPORT_BLOCK = "export"
    IMPORT = "import"
    EXTERN_FUNCTION = "extern function"
    STATIC_METHOD = "static method"


class Visibility(Enum):
    PUBLIC = "public"
    PROTECT = "protect"
    PRIVATE = "private"
    DEFAULT = ""


@dataclass
class Entity:
    kind: EntityKind
    name: str
    visibility: Visibility = Visibility.DEFAULT
    generics: str = ""
    extends: str = ""
    implements: str = ""
    return_type: str = ""
    parameters: str = ""
    type_annotation: str = ""
    backed_type: str = ""
    backed_value: str = ""
    modifiers: list = field(default_factory=list)
    children: list = field(default_factory=list)
    line_number: int = 0


@dataclass
class FileInfo:
    path: str
    package: str = ""
    imports: list = field(default_factory=list)
    entities: list = field(default_factory=list)
    exports: list = field(default_factory=list)


VISIBILITY_KEYWORDS = {"public", "protect", "private"}
MODIFIER_KEYWORDS = {"final", "abstract", "static", "mut", "readonly", "Readonly"}
TYPE_DECLARATION_KEYWORDS = {"class", "interface", "struct", "enum"}

PRIMITIVE_TYPES = {
    "I8", "I16", "I32", "I64",
    "U8", "U16", "U32", "U64",
    "F32", "F64",
    "Int", "Float", "Double",
    "Boolean", "String", "Char", "Void",
}

GENERIC_PATTERN = re.compile(r"<([^>]+)>")
EXTENDS_PATTERN = re.compile(r"\bextends\s+(.+?)(?:\s*:|$)")
IMPLEMENTS_PATTERN = re.compile(r"\bimplements\s+(.+?)(?:\s*:|$)")
PROPERTY_PATTERN = re.compile(
    r"^(\s*)"
    r"(?:(public|protect|private)\s+)?"
    r"property\s+"
    r"(\w+)"
    r"\s*\(([^)]*)\)"
    r"(?:\s*->\s*(.+?))?"
    r"\s*:?\s*$"
)
FUNCTION_PATTERN = re.compile(
    r"^(\s*)"
    r"(?:(public|protect|private)\s+)?"
    r"(?:(static)\s+)?"
    r"function\s+"
    r"(\w+)"
    r"\s*\(([^)]*)\)"
    r"(?:\s*->\s*(.+?))?"
    r"\s*:?\s*$"
)
FIELD_PATTERN = re.compile(
    r"^(\s*)"
    r"(public|protect|private)\s+"
    r"(?:(mut|readonly)\s+)?"
    r"([A-Z]\w*(?:<[^>]+>)?(?:\?)?)\s+"
    r"(\w+)"
    r"\s*$"
)
CONST_PATTERN = re.compile(
    r"^(\s*)"
    r"(?:(public|protect|private)\s+)?"
    r"const\s+"
    r"([A-Z]\w*)\s+"
    r"(\w+)\s*=\s*(.+?)\s*$"
)
UNIT_PATTERN = re.compile(
    r"^\s+"
    r"unit\s+"
    r"(\w+)"
    r"(?:\s+(.+?))?"
    r"\s*$"
)
PACKAGE_PATTERN = re.compile(r"^package\s+(.+?)\s*$")
IMPORT_PATTERN = re.compile(r"^from\s+(\S+)\s+import\s+(.+?)\s*$")
IMPORT_SIMPLE_PATTERN = re.compile(r"^import\s+(.+?)\s*$")
EXPORT_PATTERN = re.compile(r"^export\s*\{\s*$")
EXTERN_FUNCTION_PATTERN = re.compile(
    r"^extern\s+function\s+"
    r"(\w+)"
    r"\s*\(([^)]*)\)"
    r"(?:\s*->\s*(.+?))?"
    r"\s*;\s*$"
)
TYPE_DECLARATION_PATTERN = re.compile(
    r"^(\s*)"
    r"(?:(public|protect|private)\s+)?"
    r"(?:(final|abstract)\s+)?"
    r"(?:(Readonly)\s+)?"
    r"(class|interface|struct|enum)\s+"
    r"(\w+)"
    r"((?:<[^>]+>)?)"
    r"(.*?)"
    r"\s*[:;]\s*$"
)
BACKED_ENUM_PATTERN = re.compile(
    r"^(\s*)"
    r"(?:(public|protect|private)\s+)?"
    r"enum\s+"
    r"(\w+)"
    r"\s+backed\s+"
    r"(\w+)"
    r"\s*:\s*$"
)


def get_indent_level(line):
    stripped = line.lstrip()
    if not stripped:
        return -1
    indent = len(line) - len(stripped)
    return indent


def parse_inheritance(rest_of_line):
    extends = ""
    implements = ""
    extends_match = EXTENDS_PATTERN.search(rest_of_line)
    implements_match = IMPLEMENTS_PATTERN.search(rest_of_line)
    if extends_match:
        extends_text = extends_match.group(1).strip()
        implements_pos = extends_text.find(" implements ")
        if implements_pos >= 0:
            extends = extends_text[:implements_pos].strip()
        else:
            extends = extends_text.rstrip(":").strip()
    if implements_match:
        implements = implements_match.group(1).strip().rstrip(":").strip()
    return extends, implements


def parse_file(filepath):
    file_info = FileInfo(path=str(filepath))

    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as handle:
            lines = handle.readlines()
    except (OSError, IOError):
        return file_info

    container_stack = []
    current_indent_stack = []

    line_index = 0
    while line_index < len(lines):
        line = lines[line_index]
        raw_line = line.rstrip("\n")
        stripped = raw_line.strip()
        line_number = line_index + 1

        if not stripped or stripped.startswith("#") or stripped.startswith('"""'):
            if stripped.startswith('"""') and not stripped.endswith('"""'):
                line_index += 1
                while line_index < len(lines):
                    if '"""' in lines[line_index]:
                        break
                    line_index += 1
            line_index += 1
            continue

        current_indent = get_indent_level(raw_line)

        while current_indent_stack and current_indent <= current_indent_stack[-1]:
            current_indent_stack.pop()
            if container_stack:
                container_stack.pop()

        package_match = PACKAGE_PATTERN.match(stripped)
        if package_match:
            file_info.package = package_match.group(1)
            line_index += 1
            continue

        import_match = IMPORT_PATTERN.match(stripped)
        if import_match:
            module_path = import_match.group(1)
            imported_names = import_match.group(2)
            file_info.imports.append(f"from {module_path} import {imported_names}")
            line_index += 1
            continue

        import_simple_match = IMPORT_SIMPLE_PATTERN.match(stripped)
        if import_simple_match:
            file_info.imports.append(f"import {import_simple_match.group(1)}")
            line_index += 1
            continue

        export_match = EXPORT_PATTERN.match(stripped)
        if export_match:
            line_index += 1
            while line_index < len(lines):
                export_line = lines[line_index].strip()
                if export_line == "}":
                    break
                if export_line and not export_line.startswith("#"):
                    file_info.exports.append(export_line)
                line_index += 1
            line_index += 1
            continue

        extern_match = EXTERN_FUNCTION_PATTERN.match(stripped)
        if extern_match:
            entity = Entity(
                kind=EntityKind.EXTERN_FUNCTION,
                name=extern_match.group(1),
                parameters=extern_match.group(2).strip(),
                return_type=extern_match.group(3).strip() if extern_match.group(3) else "Void",
                line_number=line_number,
            )
            target = container_stack[-1].children if container_stack else file_info.entities
            target.append(entity)
            line_index += 1
            continue

        backed_match = BACKED_ENUM_PATTERN.match(raw_line)
        if backed_match:
            visibility = Visibility(backed_match.group(2)) if backed_match.group(2) else Visibility.DEFAULT
            entity = Entity(
                kind=EntityKind.ENUM,
                name=backed_match.group(3),
                visibility=visibility,
                backed_type=backed_match.group(4),
                line_number=line_number,
            )
            target = container_stack[-1].children if container_stack else file_info.entities
            target.append(entity)
            container_stack.append(entity)
            current_indent_stack.append(current_indent)
            line_index += 1
            continue

        type_match = TYPE_DECLARATION_PATTERN.match(raw_line)
        if type_match:
            visibility_str = type_match.group(2)
            modifier = type_match.group(3)
            readonly_modifier = type_match.group(4)
            kind_str = type_match.group(5)
            name = type_match.group(6)
            generics = type_match.group(7).strip()
            rest = type_match.group(8).strip()
            is_bodyless = raw_line.rstrip().endswith(";")
            is_readonly = readonly_modifier is not None

            if kind_str == "class" and modifier == "abstract":
                kind = EntityKind.ABSTRACT_CLASS
            elif kind_str == "class" and modifier == "final" and is_readonly:
                kind = EntityKind.READONLY_CLASS
            elif kind_str == "class" and modifier == "final":
                kind = EntityKind.FINAL_CLASS
            elif kind_str == "class" and is_readonly:
                kind = EntityKind.READONLY_CLASS
            elif kind_str == "class":
                kind = EntityKind.CLASS
            elif kind_str == "interface":
                kind = EntityKind.INTERFACE
            elif kind_str == "struct":
                kind = EntityKind.STRUCT
            elif kind_str == "enum":
                kind = EntityKind.ENUM
            else:
                kind = EntityKind.CLASS

            visibility = Visibility(visibility_str) if visibility_str else Visibility.DEFAULT
            extends, implements = parse_inheritance(rest)

            modifiers = []
            if modifier and modifier not in ("abstract",):
                modifiers.append(modifier)
            if is_readonly:
                modifiers.append("Readonly")

            entity = Entity(
                kind=kind,
                name=name,
                visibility=visibility,
                generics=generics,
                extends=extends,
                implements=implements,
                modifiers=modifiers,
                line_number=line_number,
            )
            target = container_stack[-1].children if container_stack else file_info.entities
            target.append(entity)
            if not is_bodyless:
                container_stack.append(entity)
                current_indent_stack.append(current_indent)
            line_index += 1
            continue

        prop_match = PROPERTY_PATTERN.match(raw_line)
        if prop_match:
            visibility_str = prop_match.group(2)
            name = prop_match.group(3)
            params = prop_match.group(4).strip()
            return_type = prop_match.group(5).strip() if prop_match.group(5) else "Void"
            visibility = Visibility(visibility_str) if visibility_str else Visibility.DEFAULT

            entity = Entity(
                kind=EntityKind.PROPERTY,
                name=name,
                visibility=visibility,
                parameters=params,
                return_type=return_type,
                line_number=line_number,
            )
            target = container_stack[-1].children if container_stack else file_info.entities
            target.append(entity)
            line_index += 1
            continue

        func_match = FUNCTION_PATTERN.match(raw_line)
        if func_match:
            visibility_str = func_match.group(2)
            is_static = func_match.group(3) == "static"
            name = func_match.group(4)
            params = func_match.group(5).strip()
            return_type = func_match.group(6).strip() if func_match.group(6) else "Void"

            visibility = Visibility(visibility_str) if visibility_str else Visibility.DEFAULT

            if container_stack and container_stack[-1].kind in (
                EntityKind.CLASS, EntityKind.ABSTRACT_CLASS, EntityKind.FINAL_CLASS,
                EntityKind.READONLY_CLASS, EntityKind.INTERFACE, EntityKind.STRUCT,
                EntityKind.ENUM,
            ):
                kind = EntityKind.STATIC_METHOD if is_static else EntityKind.METHOD
            else:
                kind = EntityKind.FUNCTION

            modifiers = []
            if is_static:
                modifiers.append("static")

            entity = Entity(
                kind=kind,
                name=name,
                visibility=visibility,
                parameters=params,
                return_type=return_type,
                modifiers=modifiers,
                line_number=line_number,
            )
            target = container_stack[-1].children if container_stack else file_info.entities
            target.append(entity)
            line_index += 1
            continue

        const_match = CONST_PATTERN.match(raw_line)
        if const_match:
            visibility_str = const_match.group(2)
            visibility = Visibility(visibility_str) if visibility_str else Visibility.DEFAULT
            entity = Entity(
                kind=EntityKind.CONSTANT,
                name=const_match.group(4),
                visibility=visibility,
                type_annotation=const_match.group(3),
                backed_value=const_match.group(5).strip(),
                line_number=line_number,
            )
            target = container_stack[-1].children if container_stack else file_info.entities
            target.append(entity)
            line_index += 1
            continue

        unit_match = UNIT_PATTERN.match(raw_line)
        if unit_match:
            entity = Entity(
                kind=EntityKind.ENUM_VARIANT,
                name=unit_match.group(1),
                backed_value=unit_match.group(2).strip() if unit_match.group(2) else "",
                line_number=line_number,
            )
            if container_stack and container_stack[-1].kind == EntityKind.ENUM:
                container_stack[-1].children.append(entity)
            else:
                target = container_stack[-1].children if container_stack else file_info.entities
                target.append(entity)
            line_index += 1
            continue

        field_match = FIELD_PATTERN.match(raw_line)
        if field_match and container_stack:
            visibility_str = field_match.group(2)
            modifier = field_match.group(3)
            type_name = field_match.group(4)
            field_name = field_match.group(5)

            modifiers = []
            if modifier:
                modifiers.append(modifier)

            entity = Entity(
                kind=EntityKind.FIELD,
                name=field_name,
                visibility=Visibility(visibility_str),
                type_annotation=type_name,
                modifiers=modifiers,
                line_number=line_number,
            )
            container_stack[-1].children.append(entity)
            line_index += 1
            continue

        line_index += 1

    return file_info


def collect_all_files(stdlibs_path):
    all_files = []
    for root, dirs, files in os.walk(stdlibs_path):
        dirs.sort()
        for filename in sorted(files):
            if filename.endswith(".urn"):
                filepath = os.path.join(root, filename)
                all_files.append(filepath)
    return all_files


def build_directory_tree(stdlibs_path, file_infos):
    tree = {}
    for file_info in file_infos:
        rel_path = os.path.relpath(file_info.path, stdlibs_path)
        parts = Path(rel_path).parts
        current = tree
        for part in parts[:-1]:
            if part not in current:
                current[part] = {"__files__": [], "__dirs__": {}}
            current = current[part]["__dirs__"]
        filename = parts[-1]
        if "__files__" not in current:
            current["__files__"] = []
            current["__dirs__"] = {}
        current["__files__"].append(file_info)
    return tree


def format_entity_signature(entity):
    parts = []

    if entity.visibility != Visibility.DEFAULT:
        parts.append(entity.visibility.value)

    for modifier in entity.modifiers:
        parts.append(modifier)

    if entity.kind == EntityKind.CONSTANT:
        parts.append("const")
        parts.append(entity.type_annotation)
        parts.append(entity.name)
        if entity.backed_value:
            parts.append(f"= {entity.backed_value}")
        return " ".join(parts)

    if entity.kind == EntityKind.FIELD:
        parts.append(entity.type_annotation)
        parts.append(entity.name)
        return " ".join(parts)

    if entity.kind == EntityKind.ENUM_VARIANT:
        result = f"unit {entity.name}"
        if entity.backed_value:
            result += f" {entity.backed_value}"
        return result

    if entity.kind == EntityKind.PROPERTY:
        parts.append("property")
        parts.append(f"{entity.name}({entity.parameters})")
        if entity.return_type and entity.return_type != "Void":
            parts.append(f"-> {entity.return_type}")
        return " ".join(parts)

    if entity.kind in (EntityKind.FUNCTION, EntityKind.METHOD,
                       EntityKind.STATIC_METHOD, EntityKind.EXTERN_FUNCTION):
        if entity.kind == EntityKind.EXTERN_FUNCTION:
            parts.append("extern")
        parts.append("function")
        parts.append(f"{entity.name}({entity.parameters})")
        if entity.return_type and entity.return_type != "Void":
            parts.append(f"-> {entity.return_type}")
        return " ".join(parts)

    parts.append(entity.kind.value)
    parts.append(entity.name)
    if entity.generics:
        parts.append(entity.generics)
    if entity.backed_type:
        parts.append(f"backed {entity.backed_type}")
    if entity.extends:
        parts.append(f"extends {entity.extends}")
    if entity.implements:
        parts.append(f"implements {entity.implements}")
    return " ".join(parts)


def render_text_tree(stdlibs_path, file_infos, output):
    output.write("=" * 80 + "\n")
    output.write("  URANITE STANDARD LIBRARY — ENTITY TREE\n")
    output.write("=" * 80 + "\n\n")

    stats = {
        "files": 0,
        "packages": 0,
        "classes": 0,
        "abstract_classes": 0,
        "final_classes": 0,
        "readonly_classes": 0,
        "interfaces": 0,
        "structs": 0,
        "enums": 0,
        "functions": 0,
        "methods": 0,
        "properties": 0,
        "static_methods": 0,
        "fields": 0,
        "constants": 0,
        "enum_variants": 0,
        "extern_functions": 0,
        "imports": 0,
        "exports": 0,
    }

    by_directory = {}
    for file_info in file_infos:
        rel_path = os.path.relpath(file_info.path, stdlibs_path)
        dir_path = os.path.dirname(rel_path)
        if dir_path not in by_directory:
            by_directory[dir_path] = []
        by_directory[dir_path].append(file_info)

    def count_entities(entities):
        for entity in entities:
            kind = entity.kind
            if kind == EntityKind.CLASS:
                stats["classes"] += 1
            elif kind == EntityKind.ABSTRACT_CLASS:
                stats["abstract_classes"] += 1
            elif kind == EntityKind.FINAL_CLASS:
                stats["final_classes"] += 1
            elif kind == EntityKind.READONLY_CLASS:
                stats["readonly_classes"] += 1
            elif kind == EntityKind.INTERFACE:
                stats["interfaces"] += 1
            elif kind == EntityKind.STRUCT:
                stats["structs"] += 1
            elif kind == EntityKind.ENUM:
                stats["enums"] += 1
            elif kind == EntityKind.FUNCTION:
                stats["functions"] += 1
            elif kind == EntityKind.METHOD:
                stats["methods"] += 1
            elif kind == EntityKind.PROPERTY:
                stats["properties"] += 1
            elif kind == EntityKind.STATIC_METHOD:
                stats["static_methods"] += 1
            elif kind == EntityKind.FIELD:
                stats["fields"] += 1
            elif kind == EntityKind.CONSTANT:
                stats["constants"] += 1
            elif kind == EntityKind.ENUM_VARIANT:
                stats["enum_variants"] += 1
            elif kind == EntityKind.EXTERN_FUNCTION:
                stats["extern_functions"] += 1
            count_entities(entity.children)

    for file_info in file_infos:
        stats["files"] += 1
        if file_info.package:
            stats["packages"] += 1
        stats["imports"] += len(file_info.imports)
        stats["exports"] += len(file_info.exports)
        count_entities(file_info.entities)

    def render_entity_tree(entities, indent, prefix_chars):
        for entity_index, entity in enumerate(entities):
            is_last = entity_index == len(entities) - 1
            connector = "└── " if is_last else "├── "
            child_prefix = "    " if is_last else "│   "

            icon = get_entity_icon(entity.kind)
            signature = format_entity_signature(entity)
            output.write(f"{prefix_chars}{connector}{icon} {signature}\n")

            if entity.children:
                render_entity_tree(
                    entity.children,
                    indent + 1,
                    prefix_chars + child_prefix,
                )

    sorted_dirs = sorted(by_directory.keys())
    for dir_path in sorted_dirs:
        dir_files = by_directory[dir_path]
        display_path = dir_path if dir_path else "(root)"
        output.write(f"\n┌{'─' * 78}┐\n")
        output.write(f"│ \U0001f4c1 {display_path:<75}│\n")
        output.write(f"└{'─' * 78}┘\n")

        for file_info in sorted(dir_files, key=lambda fi: os.path.basename(fi.path)):
            filename = os.path.basename(file_info.path)
            package_display = f" ({file_info.package})" if file_info.package else ""
            output.write(f"\n  \U0001f4c4 {filename}{package_display}\n")

            if file_info.imports:
                output.write(f"  │\n")
                output.write(f"  ├── \U0001f4e5 Imports:\n")
                for imp in file_info.imports:
                    output.write(f"  │   ├── {imp}\n")

            if file_info.exports:
                output.write(f"  │\n")
                output.write(f"  ├── \U0001f4e4 Exports:\n")
                for exp in file_info.exports:
                    output.write(f"  │   ├── {exp}\n")

            if file_info.entities:
                output.write(f"  │\n")
                render_entity_tree(file_info.entities, 1, "  ")

    output.write(f"\n\n{'=' * 80}\n")
    output.write("  SUMMARY\n")
    output.write(f"{'=' * 80}\n\n")
    output.write(f"  Files:              {stats['files']}\n")
    output.write(f"  Packages:           {stats['packages']}\n")
    output.write(f"  Classes:            {stats['classes']}\n")
    output.write(f"  Abstract Classes:   {stats['abstract_classes']}\n")
    output.write(f"  Final Classes:      {stats['final_classes']}\n")
    output.write(f"  Readonly Classes:   {stats['readonly_classes']}\n")
    output.write(f"  Interfaces:         {stats['interfaces']}\n")
    output.write(f"  Structs:            {stats['structs']}\n")
    output.write(f"  Enums:              {stats['enums']}\n")
    output.write(f"  Functions:          {stats['functions']}\n")
    output.write(f"  Methods:            {stats['methods']}\n")
    output.write(f"  Properties:         {stats['properties']}\n")
    output.write(f"  Static Methods:     {stats['static_methods']}\n")
    output.write(f"  Extern Functions:   {stats['extern_functions']}\n")
    output.write(f"  Fields:             {stats['fields']}\n")
    output.write(f"  Constants:          {stats['constants']}\n")
    output.write(f"  Enum Variants:      {stats['enum_variants']}\n")
    output.write(f"  Imports:            {stats['imports']}\n")
    output.write(f"  Exports:            {stats['exports']}\n")
    total_entities = (
        stats["classes"] + stats["abstract_classes"] + stats["final_classes"]
        + stats["readonly_classes"] + stats["interfaces"] + stats["structs"]
        + stats["enums"] + stats["functions"] + stats["methods"]
        + stats["properties"] + stats["static_methods"]
        + stats["extern_functions"] + stats["fields"] + stats["constants"]
        + stats["enum_variants"]
    )
    output.write(f"\n  Total Entities:     {total_entities}\n")
    output.write(f"{'=' * 80}\n")


def get_entity_icon(kind):
    icons = {
        EntityKind.CLASS: "\U0001f7e6",
        EntityKind.ABSTRACT_CLASS: "\U0001f7e3",
        EntityKind.FINAL_CLASS: "\U0001f7e9",
        EntityKind.READONLY_CLASS: "\U0001f7e2",
        EntityKind.INTERFACE: "\U0001f7e1",
        EntityKind.STRUCT: "\U0001f7e7",
        EntityKind.ENUM: "\U0001f7e5",
        EntityKind.FUNCTION: "⚡",
        EntityKind.METHOD: "\U0001f527",
        EntityKind.PROPERTY: "\U0001f50d",
        EntityKind.STATIC_METHOD: "⚙️",
        EntityKind.FIELD: "\U0001f4ce",
        EntityKind.CONSTANT: "\U0001f4cc",
        EntityKind.ENUM_VARIANT: "◆",
        EntityKind.EXTERN_FUNCTION: "\U0001f517",
        EntityKind.EXPORT_BLOCK: "\U0001f4e4",
        EntityKind.IMPORT: "\U0001f4e5",
        EntityKind.PACKAGE: "\U0001f4e6",
    }
    return icons.get(kind, "•")


def entity_to_dict(entity):
    result = {
        "kind": entity.kind.value,
        "name": entity.name,
        "line": entity.line_number,
    }
    if entity.visibility != Visibility.DEFAULT:
        result["visibility"] = entity.visibility.value
    if entity.generics:
        result["generics"] = entity.generics
    if entity.extends:
        result["extends"] = entity.extends
    if entity.implements:
        result["implements"] = entity.implements
    if entity.return_type:
        result["returnType"] = entity.return_type
    if entity.parameters:
        result["parameters"] = entity.parameters
    if entity.type_annotation:
        result["type"] = entity.type_annotation
    if entity.backed_type:
        result["backedType"] = entity.backed_type
    if entity.backed_value:
        result["backedValue"] = entity.backed_value
    if entity.modifiers:
        result["modifiers"] = entity.modifiers
    if entity.children:
        result["children"] = [entity_to_dict(child) for child in entity.children]
    return result


def render_json(stdlibs_path, file_infos, output):
    tree = {}
    for file_info in file_infos:
        rel_path = os.path.relpath(file_info.path, stdlibs_path)
        file_data = {
            "path": rel_path,
        }
        if file_info.package:
            file_data["package"] = file_info.package
        if file_info.imports:
            file_data["imports"] = file_info.imports
        if file_info.exports:
            file_data["exports"] = file_info.exports
        if file_info.entities:
            file_data["entities"] = [entity_to_dict(entity) for entity in file_info.entities]
        tree[rel_path] = file_data
    json.dump(tree, output, indent=4, ensure_ascii=False)
    output.write("\n")


def render_markdown(stdlibs_path, file_infos, output):
    output.write("# Uranite Standard Library Entity Tree\n\n")

    by_directory = {}
    for file_info in file_infos:
        rel_path = os.path.relpath(file_info.path, stdlibs_path)
        dir_path = os.path.dirname(rel_path)
        if dir_path not in by_directory:
            by_directory[dir_path] = []
        by_directory[dir_path].append(file_info)

    def render_entities_md(entities, depth):
        indent = "  " * depth
        for entity in entities:
            icon = get_entity_icon(entity.kind)
            signature = format_entity_signature(entity)
            output.write(f"{indent}- {icon} `{signature}`\n")
            if entity.children:
                render_entities_md(entity.children, depth + 1)

    for dir_path in sorted(by_directory.keys()):
        dir_files = by_directory[dir_path]
        display = dir_path if dir_path else "root"
        output.write(f"\n## `{display}/`\n\n")

        for file_info in sorted(dir_files, key=lambda fi: os.path.basename(fi.path)):
            filename = os.path.basename(file_info.path)
            package_display = f" — `{file_info.package}`" if file_info.package else ""
            output.write(f"\n### `{filename}`{package_display}\n\n")

            if file_info.imports:
                output.write("**Imports:**\n")
                for imp in file_info.imports:
                    output.write(f"- `{imp}`\n")
                output.write("\n")

            if file_info.exports:
                output.write("**Exports:**\n")
                for exp in file_info.exports:
                    output.write(f"- `{exp}`\n")
                output.write("\n")

            if file_info.entities:
                output.write("**Entities:**\n")
                render_entities_md(file_info.entities, 0)
                output.write("\n")


def main():
    parser = argparse.ArgumentParser(
        description="Generate entity tree from Uranite standard library"
    )
    parser.add_argument(
        "--stdlibs",
        default=None,
        help="Path to stdlibs directory (auto-detected if not provided)",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output file path (stdout if not provided)",
    )
    parser.add_argument(
        "--format",
        choices=["text", "json", "markdown"],
        default="text",
        help="Output format (default: text)",
    )
    args = parser.parse_args()

    if args.stdlibs:
        stdlibs_path = os.path.abspath(args.stdlibs)
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(script_dir)
        stdlibs_path = os.path.join(project_root, "stdlibs")

    if not os.path.isdir(stdlibs_path):
        print(f"Error: stdlibs directory not found at {stdlibs_path}", file=sys.stderr)
        sys.exit(1)

    all_files = collect_all_files(stdlibs_path)
    print(f"Parsing {len(all_files)} files...", file=sys.stderr)

    file_infos = []
    for filepath in all_files:
        file_info = parse_file(filepath)
        file_infos.append(file_info)

    print(f"Parsed {len(file_infos)} files successfully.", file=sys.stderr)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as output:
            if args.format == "json":
                render_json(stdlibs_path, file_infos, output)
            elif args.format == "markdown":
                render_markdown(stdlibs_path, file_infos, output)
            else:
                render_text_tree(stdlibs_path, file_infos, output)
        print(f"Output written to {args.output}", file=sys.stderr)
    else:
        if args.format == "json":
            render_json(stdlibs_path, file_infos, sys.stdout)
        elif args.format == "markdown":
            render_markdown(stdlibs_path, file_infos, sys.stdout)
        else:
            render_text_tree(stdlibs_path, file_infos, sys.stdout)


if __name__ == "__main__":
    main()
