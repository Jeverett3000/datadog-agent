import argparse
import json
from dataclasses import dataclass
from typing import List

import common


@dataclass
class EventTypeProperty:
    name: str
    datatype: str
    definition: str
    constants: str


@dataclass
class EventType:
    name: str
    kind: str
    definition: str
    min_agent_version: str
    experimental: bool
    properties: List[EventTypeProperty]


@dataclass
class Constant:
    name: str
    architecture: str


@dataclass
class Constants:
    name: str
    definition: str
    all: List[Constant]


def build_event_types(top_node):
    output = []
    for et in top_node["event_types"]:
        event_type = EventType(
            et["name"], et["type"], et["definition"], et["from_agent_version"], et["experimental"], []
        )
        for p in et["properties"]:
            try:
                prop = EventTypeProperty(p["name"], p["type"], p["definition"], p["constants"])
            except KeyError:
                prop = EventTypeProperty(p["name"], p["type"], p["definition"], "none")
            event_type.properties.append(prop)
        output.append(event_type)
    return output


def build_constants(top_node):
    output = []
    for cs in top_node["constants"]:
        constants = Constants(cs["name"], cs["description"], [])
        for c in cs["all"]:
            constants.all.append(Constant(c["name"], c["architecture"]))
        output.append(constants)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate SECL documentation")
    parser.add_argument("--input", type=str, help="input json file generated by the accessors generator")
    parser.add_argument("--output", type=str, help="output file")
    parser.add_argument("--template", type=str, default="agent_expressions.md", help="template used for the generation")
    args = parser.parse_args()

    with open(args.input) as secl_json_file:
        json_top_node = json.load(secl_json_file)
    event_types = build_event_types(json_top_node)
    constants_list = build_constants(json_top_node)

    with open(args.output, "w") as output_file:
        print(common.fill_template(args.template, event_types=event_types, constants_list=constants_list), file=output_file)
