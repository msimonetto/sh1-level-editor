#!/usr/bin/env python3
"""
pack_overlay_to_decomp.py — Packs editor overlay JSON files (map_points.json & events.json)
back into Silent Hill 1 decompilation C source files (map_points.h & <map>_events_data.c).
"""

import os
import sys
import json
import argparse

WORKSPACE_OVERLAYS_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'data', 'workspace', 'overlays'))
DECOMP_MAPS_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'external', 'SlickAmogus_silent-hill-decomp', 'src', 'maps'))

def format_q12(val_float):
    target_q12 = int(round(val_float * 4096.0))
    chosen = round(val_float, 4)
    for decimals in [0, 1, 2, 3]:
        candidate = round(val_float, decimals)
        if int(round(candidate * 4096.0)) == target_q12:
            chosen = candidate
            break

    s = f"{chosen:.4f}".rstrip('0').rstrip('.') if '.' in f"{chosen:.4f}" else f"{chosen}"
    if '.' not in s:
        s += ".0"
    return f"Q12({s}f)"

def generate_map_points_h(map_key, waypoints):
    lines = []
    num_pts = len(waypoints)
    for i, wp in enumerate(waypoints):
        lines.append("    {")
        lines.append(f"        .positionX = {format_q12(wp.get('worldX', 0.0))},")
        lines.append(f"        .paperMapIdx = {wp.get('paperMapIdx', 'PaperMapIdx_OtherPlaces')},")
        lines.append(f"        .field_4_5 = {wp.get('field_4_5', 0)},")
        lines.append(f"        .loadingScreenId = {wp.get('loadingScreenId', 'LoadingScreenId_None')},")
        lines.append(f"        .unused_4_12 = {wp.get('unused_4_12', 0)},")
        lines.append(f"        .triggerParam0 = {wp.get('triggerParam0', 0)},")
        lines.append(f"        .triggerParam1 = {wp.get('triggerParam1', 0)},")
        lines.append(f"        .positionZ = {format_q12(wp.get('worldZ', 0.0))},")
        if i == num_pts - 1:
            lines.append("    }")
        else:
            lines.append("    },")
    return "\n".join(lines) + "\n"

def generate_events_data_c(map_key, events, existing_header=""):
    lines = []
    if existing_header:
        lines.append(existing_header.rstrip())
        lines.append("")
    else:
        lines.append('#include "bodyprog/bodyprog.h"')
        lines.append("")

    # Ensure array terminates with TriggerType_EndOfArray sentinel struct
    events_list = list(events)
    if not events_list or events_list[-1].get('triggerType') != 'TriggerType_EndOfArray':
        events_list.append({ "triggerType": "TriggerType_EndOfArray", "triggerTypeValue": 15 })

    lines.append(f"s_EventData MAP_EVENTS[{len(events_list)}] = {{")
    for ev in events_list:
        trig_type = ev.get('triggerType', 'TriggerType_None')
        if trig_type == 'TriggerType_EndOfArray' or ev.get('triggerTypeValue') == 15:
            lines.append("    {")
            lines.append("        .triggerType = TriggerType_EndOfArray,")
            lines.append("    },")
            continue

        lines.append("    {")
        req_flag = ev.get('requiredEventFlag', 0)
        if req_flag != 0:
            lines.append(f"        .requiredEventFlag = {req_flag},")
            
        dis_flag = ev.get('disabledEventFlag', '0')
        if dis_flag != 0 and dis_flag != '0' and dis_flag != 'EventFlag_None':
            lines.append(f"        .disabledEventFlag = {dis_flag},")

        lines.append(f"        .triggerType = {trig_type},")

        act_type = ev.get('activationType', 'TriggerActivationType_None')
        if act_type != 'TriggerActivationType_None':
            lines.append(f"        .activationType = {act_type},")

        poi = ev.get('pointOfInterestIdx', 0)
        if poi != 0:
            lines.append(f"        .pointOfInterestIdx = {poi},")

        req_item = ev.get('requiredItemId', 'InvItemId_None')
        if req_item != 'InvItemId_None':
            lines.append(f"        .requiredItemId = {req_item},")

        sys_state = ev.get('sysState', 'SysState_EventCallback')
        lines.append(f"        .sysState = {sys_state},")

        param = ev.get('eventParam', 0)
        if param != 0:
            lines.append(f"        .eventParam = {param},")

        flags_8_13 = ev.get('flags_8_13', 0)
        if flags_8_13 != 0:
            lines.append(f"        .flags_8_13 = {flags_8_13},")

        sfx = ev.get('sfxPairIdx', 'SfxPairIdx_None')
        if sfx != 'SfxPairIdx_None':
            lines.append(f"        .sfxPairIdx = {sfx},")

        f_8_24 = ev.get('field_8_24', 0)
        if f_8_24 != 0:
            lines.append(f"        .field_8_24 = {f_8_24},")

        dest_map = ev.get('mapIdx', 'MapIdx_None')
        if dest_map != 'MapIdx_None':
            lines.append(f"        .mapIdx = {dest_map},")

        lines.append("    },")
    lines.append("};\n")
    return "\n".join(lines)

def pack_map(map_key):
    map_dir_name = map_key.lower()
    decomp_map_dir = os.path.join(DECOMP_MAPS_DIR, map_dir_name)
    json_map_dir = os.path.join(WORKSPACE_OVERLAYS_DIR, map_key)

    if not os.path.exists(json_map_dir):
        print(f"[pack_overlay] Error: JSON overlay directory '{json_map_dir}' does not exist.")
        return False

    pts_json_path = os.path.join(json_map_dir, 'map_points.json')
    evs_json_path = os.path.join(json_map_dir, 'events.json')

    if not os.path.exists(pts_json_path) or not os.path.exists(evs_json_path):
        print(f"[pack_overlay] Error: Missing map_points.json or events.json in '{json_map_dir}'.")
        return False

    with open(pts_json_path, 'r') as f:
        waypoints = json.load(f)

    with open(evs_json_path, 'r') as f:
        events = json.load(f)

    if not os.path.exists(decomp_map_dir):
        os.makedirs(decomp_map_dir, exist_ok=True)

    # 1. Write map_points.h
    target_pts_h = os.path.join(decomp_map_dir, 'map_points.h')
    pts_content = generate_map_points_h(map_key, waypoints)
    with open(target_pts_h, 'w') as f:
        f.write(pts_content)

    # 2. Write <map_dir>_events_data.c (preserving header if present)
    target_evs_c = os.path.join(decomp_map_dir, f"{map_dir_name}_events_data.c")
    existing_header = ""
    if os.path.exists(target_evs_c):
        with open(target_evs_c, 'r') as f:
            old_code = f.read()
        idx = old_code.find('s_EventData MAP_EVENTS')
        if idx != -1:
            existing_header = old_code[:idx]
        else:
            existing_header = old_code

    evs_content = generate_events_data_c(map_key, events, existing_header)
    with open(target_evs_c, 'w') as f:
        f.write(evs_content)

    num_evs = len(events)
    if not events or events[-1].get('triggerType') != 'TriggerType_EndOfArray':
        num_evs += 1

    print(f"[pack_overlay] Successfully packed {map_key} -> {decomp_map_dir} ({len(waypoints)} pts, {num_evs} evs)")
    return True

def main():
    parser = argparse.ArgumentParser(description="Pack editor JSON overlays back into Silent Hill 1 decomp C source files.")
    parser.add_argument('--map', help="Map key to pack (e.g. MAP0_S00)")
    parser.add_argument('--all', action='store_true', help="Pack all 43 maps")

    args = parser.parse_args()

    if args.all:
        maps = [d for d in os.listdir(WORKSPACE_OVERLAYS_DIR) if os.path.isdir(os.path.join(WORKSPACE_OVERLAYS_DIR, d))]
        success_count = 0
        for m in sorted(maps):
            if pack_map(m):
                success_count += 1
        print(f"[pack_overlay] Finished repacking {success_count}/{len(maps)} maps to decomp source.")
    elif args.map:
        pack_map(args.map)
    else:
        parser.print_help()

if __name__ == '__main__':
    main()
