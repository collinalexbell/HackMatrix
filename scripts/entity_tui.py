import os
import string
import sys
import termios
import tty
from dataclasses import dataclass
from typing import List, Optional, Tuple

import zmq
from rich.console import Console
from rich.layout import Layout
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

import hackMatrix.protos.api_pb2 as api_pb2


DEFAULT_ADDRESS = os.getenv("VOXEL_API_ADDRESS", "tcp://127.0.0.1:3345")


@dataclass
class EntityInfo:
    entity_id: int
    component_types: List[int]


class ApiClient:
    def __init__(self, address: str) -> None:
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(address)

    def close(self) -> None:
        self.socket.close()
        self.context.term()

    def send(self, request: api_pb2.ApiRequest) -> api_pb2.ApiRequestResponse:
        serialized = request.SerializeToString()
        self.socket.send(serialized)
        data = self.socket.recv()
        response = api_pb2.ApiRequestResponse()
        response.ParseFromString(data)
        return response

    def list_entities(self, filter_type: int) -> List[EntityInfo]:
        request = api_pb2.ApiRequest(
            entityId=0,
            type=api_pb2.LIST_ENTITIES,
            listEntities=api_pb2.ListEntities(filter_type=filter_type),
        )
        response = self.send(request)
        if not response.success:
            return []
        entities: List[EntityInfo] = []
        for info in response.entity_components:
            entities.append(
                EntityInfo(
                    entity_id=info.entity_id,
                    component_types=list(info.component_types),
                )
            )
        entities.sort(key=lambda e: e.entity_id)
        return entities

    def create_entity(self) -> bool:
        request = api_pb2.ApiRequest(
            entityId=0,
            type=api_pb2.CREATE_ENTITY,
            createEntity=api_pb2.CreateEntity(),
        )
        response = self.send(request)
        return response.success

    def get_component(self, entity_id: int, component_type: int) -> Optional[api_pb2.Component]:
        request = api_pb2.ApiRequest(
            entityId=entity_id,
            type=api_pb2.GET_COMPONENT,
            getComponent=api_pb2.GetComponent(component_type=component_type),
        )
        response = self.send(request)
        if not response.success:
            return None
        return response.component

    def edit_positionable(
        self,
        entity_id: int,
        position: Tuple[float, float, float],
        rotation: Tuple[float, float, float],
        origin: Tuple[float, float, float],
        scale: float,
    ) -> bool:
        component = api_pb2.Component(
            type=api_pb2.COMPONENT_TYPE_POSITIONABLE,
            positionable=api_pb2.PositionableComponent(
                position=api_pb2.Vector(x=position[0], y=position[1], z=position[2]),
                rotation=api_pb2.Vector(x=rotation[0], y=rotation[1], z=rotation[2]),
                origin=api_pb2.Vector(x=origin[0], y=origin[1], z=origin[2]),
                scale=scale,
            ),
        )
        request = api_pb2.ApiRequest(
            entityId=entity_id,
            type=api_pb2.EDIT_COMPONENT,
            editComponent=api_pb2.EditComponent(component=component),
        )
        response = self.send(request)
        return response.success

    def add_positionable(
        self,
        entity_id: int,
        position: Tuple[float, float, float],
        rotation: Tuple[float, float, float],
        origin: Tuple[float, float, float],
        scale: float,
    ) -> bool:
        component = api_pb2.Component(
            type=api_pb2.COMPONENT_TYPE_POSITIONABLE,
            positionable=api_pb2.PositionableComponent(
                position=api_pb2.Vector(x=position[0], y=position[1], z=position[2]),
                rotation=api_pb2.Vector(x=rotation[0], y=rotation[1], z=rotation[2]),
                origin=api_pb2.Vector(x=origin[0], y=origin[1], z=origin[2]),
                scale=scale,
            ),
        )
        request = api_pb2.ApiRequest(
            entityId=entity_id,
            type=api_pb2.ADD_COMPONENT,
            addComponent=api_pb2.AddComponent(component=component),
        )
        response = self.send(request)
        return response.success

    def edit_model(self, entity_id: int, model_path: str) -> bool:
        component = api_pb2.Component(
            type=api_pb2.COMPONENT_TYPE_MODEL,
            model=api_pb2.ModelComponent(model_path=model_path),
        )
        request = api_pb2.ApiRequest(
            entityId=entity_id,
            type=api_pb2.EDIT_COMPONENT,
            editComponent=api_pb2.EditComponent(component=component),
        )
        response = self.send(request)
        return response.success

    def add_model(self, entity_id: int, model_path: str) -> bool:
        if not model_path:
            return False
        component = api_pb2.Component(
            type=api_pb2.COMPONENT_TYPE_MODEL,
            model=api_pb2.ModelComponent(model_path=model_path),
        )
        request = api_pb2.ApiRequest(
            entityId=entity_id,
            type=api_pb2.ADD_COMPONENT,
            addComponent=api_pb2.AddComponent(component=component),
        )
        response = self.send(request)
        return response.success


def format_component_types(component_types: List[int]) -> str:
    labels = []
    for ctype in component_types:
        if ctype == api_pb2.COMPONENT_TYPE_POSITIONABLE:
            labels.append("Positionable")
        elif ctype == api_pb2.COMPONENT_TYPE_MODEL:
            labels.append("Model")
    return ", ".join(labels) if labels else "-"


def get_key() -> str:
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == "\x1b":
            seq = sys.stdin.read(2)
            return ch + seq
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)


def component_type_label(filter_type: int) -> str:
    if filter_type == api_pb2.COMPONENT_TYPE_POSITIONABLE:
        return "Positionable"
    if filter_type == api_pb2.COMPONENT_TYPE_MODEL:
        return "Model"
    return "All"


@dataclass
class EditField:
    key: str
    label: str
    values: List[str]
    numeric: bool
    component: str


@dataclass
class EditState:
    fields: List[EditField]
    entity_id: int
    has_positionable: bool
    has_model: bool
    index: int = 0
    sub_index: int = 0
    buffer: str = ""


def build_edit_fields(
    positionable: Optional[api_pb2.PositionableComponent],
    model: Optional[api_pb2.ModelComponent],
) -> List[EditField]:
    fields: List[EditField] = []
    if positionable:
        fields.extend(
            [
                EditField(
                    "position",
                    "Position",
                    [f"{positionable.position.x}", f"{positionable.position.y}", f"{positionable.position.z}"],
                    True,
                    "positionable",
                ),
                EditField(
                    "rotation",
                    "Rotation",
                    [f"{positionable.rotation.x}", f"{positionable.rotation.y}", f"{positionable.rotation.z}"],
                    True,
                    "positionable",
                ),
                EditField(
                    "origin",
                    "Origin",
                    [f"{positionable.origin.x}", f"{positionable.origin.y}", f"{positionable.origin.z}"],
                    True,
                    "positionable",
                ),
                EditField("scale", "Scale", [f"{positionable.scale}"], True, "positionable"),
            ]
        )
    if model:
        fields.append(EditField("model_path", "Model Path", [model.model_path or ""], False, "model"))
    return fields


def get_field(edit_state: EditState, key: str) -> Optional[EditField]:
    for field in edit_state.fields:
        if field.key == key:
            return field
    return None


def apply_edit_state(edit_state: EditState, client: ApiClient) -> Tuple[bool, str]:
    try:
        message_parts: List[str] = []
        ok = True
        position_field = get_field(edit_state, "position")
        origin_field = get_field(edit_state, "origin")
        rotation_field = get_field(edit_state, "rotation")
        scale_field = get_field(edit_state, "scale")
        if position_field and origin_field and rotation_field and scale_field:
            pos = tuple(float(v) for v in position_field.values)
            origin = tuple(float(v) for v in origin_field.values)
            rot = tuple(float(v) for v in rotation_field.values)
            scale = float(scale_field.values[0])
            if edit_state.has_positionable:
                pos_ok = client.edit_positionable(edit_state.entity_id, pos, rot, origin, scale)
                message_parts.append(
                    "Positionable updated" if pos_ok else "Positionable update failed"
                )
            else:
                pos_ok = client.add_positionable(edit_state.entity_id, pos, rot, origin, scale)
                message_parts.append(
                    "Positionable added" if pos_ok else "Positionable add failed"
                )
                if pos_ok:
                    edit_state.has_positionable = True
            ok = pos_ok and ok
        model_field = get_field(edit_state, "model_path")
        if model_field:
            path = model_field.values[0]
            if not path:
                return False, "Model path required"
            if edit_state.has_model:
                model_ok = client.edit_model(edit_state.entity_id, path)
                message_parts.append("Model updated" if model_ok else "Model update failed")
            else:
                model_ok = client.add_model(edit_state.entity_id, path)
                message_parts.append("Model added" if model_ok else "Model add failed")
                if model_ok:
                    edit_state.has_model = True
            ok = ok and model_ok
        if message_parts:
            return ok, " | ".join(message_parts)
    except ValueError:
        return False, "Invalid numeric value"
    return False, "No changes applied"

def build_layout() -> Layout:
    layout = Layout()
    layout.split_column(
        Layout(name="header", size=3),
        Layout(name="body", ratio=1),
        Layout(name="footer", size=3),
    )
    layout["body"].split_row(
        Layout(name="entities", ratio=2),
        Layout(name="details", ratio=3),
    )
    return layout


def render_entities_table(
    entities: List[EntityInfo], selected_index: int
) -> Table:
    table = Table(expand=True, show_header=True, header_style="bold cyan")
    table.add_column("Entity ID", justify="right", no_wrap=True)
    table.add_column("Components", justify="left")
    for idx, entity in enumerate(entities):
        style = "reverse" if idx == selected_index else ""
        table.add_row(
            str(entity.entity_id),
            format_component_types(entity.component_types),
            style=style,
        )
    if not entities:
        table.add_row("-", "No entities", style="dim")
    return table


def get_entity_list_height(console: Console) -> int:
    height = console.size.height
    body_height = max(1, height - 6)
    content_height = max(1, body_height - 2)
    return max(1, content_height - 1)


def render_details_panel(
    entity_id: Optional[int],
    positionable: Optional[api_pb2.PositionableComponent],
    model: Optional[api_pb2.ModelComponent],
    edit_state: Optional[EditState],
    status_message: str,
) -> Panel:
    text = Text()
    if entity_id is None:
        text.append("No entity selected.", style="dim")
        return Panel(text, title="Details", border_style="blue")
    text.append(f"Entity ID: {entity_id}\n\n", style="bold")
    active_key = None
    active_sub = None
    if edit_state:
        active_field = edit_state.fields[edit_state.index]
        active_key = active_field.key
        active_sub = edit_state.sub_index

    def append_tuple_line(
        label: str,
        values: List[str],
        active_match: bool,
        active_index: int,
    ) -> None:
        text.append(f"  {label}: (")
        for idx, value in enumerate(values):
            if idx > 0:
                text.append(", ")
            display = value
            if active_match and idx == active_index and edit_state and edit_state.buffer:
                display = edit_state.buffer
            if active_match and idx == active_index:
                text.append(display, style="reverse")
            else:
                text.append(display)
        text.append(")\n")

    def append_scalar_line(label: str, value: str, active_match: bool) -> None:
        display = value
        if active_match and edit_state and edit_state.buffer:
            display = edit_state.buffer
        text.append(f"  {label}: ")
        if active_match:
            text.append(display, style="reverse")
        else:
            text.append(display)
        text.append("\n")

    position_field = get_field(edit_state, "position") if edit_state else None
    origin_field = get_field(edit_state, "origin") if edit_state else None
    rotation_field = get_field(edit_state, "rotation") if edit_state else None
    scale_field = get_field(edit_state, "scale") if edit_state else None
    show_positionable = positionable or any(
        field is not None for field in (position_field, origin_field, rotation_field, scale_field)
    )
    if show_positionable:
        text.append("Positionable\n", style="bold green")
        if position_field:
            append_tuple_line(
                "Position",
                position_field.values,
                active_key == "position",
                active_sub or 0,
            )
        elif positionable:
            append_tuple_line(
                "Position",
                [
                    f"{positionable.position.x:.2f}",
                    f"{positionable.position.y:.2f}",
                    f"{positionable.position.z:.2f}",
                ],
                False,
                0,
            )
        if rotation_field:
            append_tuple_line(
                "Rotation",
                rotation_field.values,
                active_key == "rotation",
                active_sub or 0,
            )
        elif positionable:
            append_tuple_line(
                "Rotation",
                [
                    f"{positionable.rotation.x:.2f}",
                    f"{positionable.rotation.y:.2f}",
                    f"{positionable.rotation.z:.2f}",
                ],
                False,
                0,
            )
        if origin_field:
            append_tuple_line(
                "Origin",
                origin_field.values,
                active_key == "origin",
                active_sub or 0,
            )
        elif positionable:
            append_tuple_line(
                "Origin",
                [
                    f"{positionable.origin.x:.2f}",
                    f"{positionable.origin.y:.2f}",
                    f"{positionable.origin.z:.2f}",
                ],
                False,
                0,
            )
        if scale_field:
            append_scalar_line(
                "Scale",
                scale_field.values[0],
                active_key == "scale",
            )
        elif positionable:
            append_scalar_line(
                "Scale",
                f"{positionable.scale:.2f}",
                False,
            )
        text.append("\n")
    else:
        text.append("Positionable: not present\n\n", style="dim")

    model_field = get_field(edit_state, "model_path") if edit_state else None
    if model or model_field:
        text.append("Model\n", style="bold green")
        if model_field:
            append_scalar_line("Path", model_field.values[0], active_key == "model_path")
        elif model:
            text.append(f"  Path: {model.model_path}\n")
    else:
        text.append("Model: not present\n", style="dim")
    if edit_state:
        text.append("\nEditing (type to change; Enter or arrows commit)\n", style="dim")
    if status_message:
        text.append(f"\n{status_message}\n", style="cyan")
    return Panel(text, title="Details", border_style="blue")


def main() -> int:
    console = Console()
    client = ApiClient(DEFAULT_ADDRESS)

    filter_cycle = [
        api_pb2.COMPONENT_TYPE_UNSPECIFIED,
        api_pb2.COMPONENT_TYPE_POSITIONABLE,
        api_pb2.COMPONENT_TYPE_MODEL,
    ]
    filter_index = 0
    selected_index = 0
    edit_state: Optional[EditState] = None
    status_message = ""
    scroll_offset = 0

    def refresh_entities() -> List[EntityInfo]:
        return client.list_entities(filter_cycle[filter_index])

    entities = refresh_entities()

    def get_selected_entity_id() -> Optional[int]:
        if not entities:
            return None
        return entities[selected_index].entity_id

    def fetch_components(entity_id: int) -> Tuple[Optional[api_pb2.PositionableComponent], Optional[api_pb2.ModelComponent]]:
        positionable_component = client.get_component(
            entity_id, api_pb2.COMPONENT_TYPE_POSITIONABLE
        )
        model_component = client.get_component(entity_id, api_pb2.COMPONENT_TYPE_MODEL)
        positionable = (
            positionable_component.positionable
            if positionable_component and positionable_component.HasField("positionable")
            else None
        )
        model = (
            model_component.model
            if model_component and model_component.HasField("model")
            else None
        )
        return positionable, model

    layout = build_layout()
    help_text = Text(
        "↑/↓ select  f filter  r refresh  n new entity  p add positionable  m add model  → edit  ← back  q quit",
        style="dim",
    )

    def clamp_scroll_offset(max_rows: int) -> None:
        nonlocal scroll_offset
        if not entities:
            scroll_offset = 0
            return
        if selected_index < scroll_offset:
            scroll_offset = selected_index
        elif selected_index >= scroll_offset + max_rows:
            scroll_offset = selected_index - max_rows + 1
        max_offset = max(0, len(entities) - max_rows)
        scroll_offset = min(max(scroll_offset, 0), max_offset)

    with Live(layout, console=console, screen=True, auto_refresh=False) as live:
        while True:
            selected_id = get_selected_entity_id()
            positionable = None
            model = None
            if selected_id is not None:
                positionable, model = fetch_components(selected_id)

            header = Text(
                f"Entity TUI  |  Filter: {component_type_label(filter_cycle[filter_index])}",
                style="bold cyan",
            )
            layout["header"].update(Panel(header, border_style="blue"))
            max_rows = get_entity_list_height(console)
            clamp_scroll_offset(max_rows)
            visible_entities = entities[scroll_offset : scroll_offset + max_rows]
            visible_selected = selected_index - scroll_offset if entities else 0
            if visible_entities:
                start = scroll_offset
                end = scroll_offset + len(visible_entities) - 1
                entities_title = f"Entities ({start}-{end} / {len(entities) - 1})"
            else:
                entities_title = "Entities"
            layout["entities"].update(
                Panel(
                    render_entities_table(visible_entities, visible_selected),
                    title=entities_title,
                )
            )
            layout["details"].update(
                render_details_panel(
                    selected_id, positionable, model, edit_state, status_message
                )
            )
            layout["footer"].update(Panel(help_text, border_style="blue"))
            live.refresh()

            key = get_key()
            if key in ("q", "Q"):
                break
            if edit_state:
                def commit_buffer() -> Tuple[bool, str]:
                    if edit_state.buffer == "":
                        return True, ""
                    field = edit_state.fields[edit_state.index]
                    if field.numeric and edit_state.buffer in ("-", ".", "-."):
                        return False, "Invalid numeric value"
                    if len(field.values) > 1:
                        field.values[edit_state.sub_index] = edit_state.buffer
                    else:
                        field.values[0] = edit_state.buffer
                    edit_state.buffer = ""
                    return apply_edit_state(edit_state, client)

                if key == "\x1b":  # escape
                    edit_state = None
                    status_message = "Edit cancelled"
                    continue
                if key == "\x1b[C":  # right arrow
                    ok, message = commit_buffer()
                    if not ok:
                        status_message = message
                        continue
                    field = edit_state.fields[edit_state.index]
                    if len(field.values) > 1 and edit_state.sub_index < len(field.values) - 1:
                        edit_state.sub_index += 1
                    else:
                        if edit_state.index < len(edit_state.fields) - 1:
                            edit_state.index += 1
                            next_field = edit_state.fields[edit_state.index]
                            edit_state.sub_index = 0 if len(next_field.values) > 1 else 0
                    if message:
                        status_message = message
                    continue
                if key == "\x1b[D":  # left arrow
                    ok, message = commit_buffer()
                    if not ok:
                        status_message = message
                        continue
                    field = edit_state.fields[edit_state.index]
                    if len(field.values) > 1 and edit_state.sub_index > 0:
                        edit_state.sub_index -= 1
                    else:
                        if edit_state.index > 0:
                            edit_state.index -= 1
                            prev_field = edit_state.fields[edit_state.index]
                            edit_state.sub_index = (
                                len(prev_field.values) - 1 if len(prev_field.values) > 1 else 0
                            )
                        else:
                            edit_state = None
                            status_message = message or "Edit complete"
                            continue
                    if message:
                        status_message = message
                    continue
                if key == "\x1b[A":  # up arrow
                    ok, message = commit_buffer()
                    if not ok:
                        status_message = message
                        continue
                    if edit_state.index > 0:
                        edit_state.index -= 1
                        edit_state.sub_index = 0
                    if message:
                        status_message = message
                    continue
                if key == "\x1b[B":  # down arrow
                    ok, message = commit_buffer()
                    if not ok:
                        status_message = message
                        continue
                    if edit_state.index < len(edit_state.fields) - 1:
                        edit_state.index += 1
                        edit_state.sub_index = 0
                    if message:
                        status_message = message
                    continue
                if key == "\t":  # tab
                    ok, message = commit_buffer()
                    if not ok:
                        status_message = message
                        continue
                    edit_state.index = (edit_state.index + 1) % len(edit_state.fields)
                    next_field = edit_state.fields[edit_state.index]
                    edit_state.sub_index = 0 if len(next_field.values) > 1 else 0
                    if message:
                        status_message = message
                    continue
                if key == "\x1b[Z":  # shift+tab
                    ok, message = commit_buffer()
                    if not ok:
                        status_message = message
                        continue
                    edit_state.index = (edit_state.index - 1) % len(edit_state.fields)
                    prev_field = edit_state.fields[edit_state.index]
                    edit_state.sub_index = (
                        len(prev_field.values) - 1 if len(prev_field.values) > 1 else 0
                    )
                    if message:
                        status_message = message
                    continue
                if key in ("\r", "\n"):
                    ok, message = commit_buffer()
                    status_message = message
                    continue
                if key in ("\x7f", "\b"):
                    edit_state.buffer = edit_state.buffer[:-1]
                    continue
                if len(key) == 1:
                    field = edit_state.fields[edit_state.index]
                    if field.numeric:
                        if key in "-.0123456789":
                            if edit_state.buffer == "":
                                edit_state.buffer = ""
                            edit_state.buffer += key
                    else:
                        if key in string.printable and key not in "\r\n\t":
                            if edit_state.buffer == "":
                                edit_state.buffer = ""
                            edit_state.buffer += key
                continue
            if key in ("\x1b[A", "k"):
                if entities:
                    selected_index = max(0, selected_index - 1)
            elif key in ("\x1b[B", "j"):
                if entities:
                    selected_index = min(len(entities) - 1, selected_index + 1)
            elif key in ("f", "F"):
                filter_index = (filter_index + 1) % len(filter_cycle)
                entities = refresh_entities()
                selected_index = 0
                scroll_offset = 0
            elif key in ("r", "R"):
                entities = refresh_entities()
                selected_index = min(selected_index, max(len(entities) - 1, 0))
                scroll_offset = min(scroll_offset, max(len(entities) - 1, 0))
            elif key in ("n", "N"):
                ok = client.create_entity()
                if ok:
                    entities = refresh_entities()
                    if entities:
                        newest_id = max(entity.entity_id for entity in entities)
                        for idx, entity in enumerate(entities):
                            if entity.entity_id == newest_id:
                                selected_index = idx
                                break
                    if filter_cycle[filter_index] != api_pb2.COMPONENT_TYPE_UNSPECIFIED:
                        status_message = (
                            "Entity created. It may be hidden by filter until it has a component."
                        )
                    else:
                        status_message = "Entity created"
                else:
                    status_message = "Entity create failed"
            elif key in ("p", "P") and selected_id is not None:
                if positionable:
                    status_message = "Positionable already present (use → to edit)"
                    continue
                default_positionable = api_pb2.PositionableComponent(
                    position=api_pb2.Vector(x=0.0, y=0.0, z=0.0),
                    rotation=api_pb2.Vector(x=0.0, y=0.0, z=0.0),
                    origin=api_pb2.Vector(x=0.0, y=0.0, z=0.0),
                    scale=1.0,
                )
                fields = build_edit_fields(default_positionable, model)
                if not fields:
                    status_message = "Positionable add failed"
                    continue
                edit_state = EditState(
                    fields=fields,
                    entity_id=selected_id,
                    has_positionable=False,
                    has_model=model is not None,
                )
                status_message = "Add Positionable (type to change; Enter to save)"
            elif key in ("m", "M") and selected_id is not None:
                if model:
                    status_message = "Model already present (use → to edit)"
                    continue
                default_model = api_pb2.ModelComponent(model_path="")
                fields = build_edit_fields(positionable, default_model)
                if not fields:
                    status_message = "Model add failed"
                    continue
                edit_state = EditState(
                    fields=fields,
                    entity_id=selected_id,
                    has_positionable=positionable is not None,
                    has_model=False,
                )
                status_message = "Add Model (type path; Enter to save)"
            elif key == "\x1b[C" and selected_id is not None:  # right arrow
                fields = build_edit_fields(positionable, model)
                if not fields:
                    status_message = "No editable components for this entity"
                    continue
                edit_state = EditState(
                    fields=fields,
                    entity_id=selected_id,
                    has_positionable=positionable is not None,
                    has_model=model is not None,
                )
                status_message = "Editing components"

    client.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
