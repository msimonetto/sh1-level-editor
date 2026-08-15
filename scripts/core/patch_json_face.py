import sys
import json
import os
import argparse

def patch_face(args):
    if not os.path.exists(args.json_file):
        print(f"Error: {args.json_file} does not exist.")
        sys.exit(1)
        
    with open(args.json_file, 'r') as f:
        data = json.load(f)
        
    plm = data.get("ipd_plm")
    if not plm:
        print("Error: No 'ipd_plm' found in JSON.")
        sys.exit(1)
        
    objects = plm.get("obj_headers", [])
    
    obj = next((o for o in objects if o.get("name") == args.objName), None)
    if not obj:
        print(f"Error: Object with name '{args.objName}' not found in IPD PLM.")
        sys.exit(1)
        
    meshes = obj.get("meshes", [])
    if args.mesh < 0 or args.mesh >= len(meshes):
        print(f"Error: Mesh index out of bounds ({args.mesh}).")
        sys.exit(1)
        
    mesh = meshes[args.mesh]
    packs = mesh.get("packs", [])
    if args.face < 0 or args.face >= len(packs):
        print(f"Error: Face index out of bounds ({args.face}).")
        sys.exit(1)
        
    pack = packs[args.face]
    
    if args.tex is not None:
        tex_names = plm.get("tex_names", [])
        if args.tex in tex_names:
            tex_idx = tex_names.index(args.tex)
        else:
            tex_idx = len(tex_names)
            tex_names.append(args.tex)
            padded = args.tex.encode('ascii')[:23] + b'\0'
            padded += b'\0' * (24 - len(padded))
            plm.setdefault("tex_names_raw_hex", []).append(padded.hex())
            plm["tex_names"] = tex_names
            plm["plm_header"]["tex_num"] = len(tex_names)
            
        tex_byte = pack.get("tex_num_and_unk2_byte", 0)
        tex_byte = (tex_byte & ~0x7F) | (tex_idx & 0x7F)
        pack["tex_num_and_unk2_byte"] = tex_byte

    if args.clut is not None:
        if "cba" in pack:
            cba = pack["cba"]
            cba = (cba & ~0x7FC0) | ((args.clut << 6) & 0x7FC0)
            pack["cba"] = cba

    if args.uv is not None:
        # Convert 0.0-1.0 floats back to 0-255 ints.
        # But for PS1, the max UV coordinate in a quad is typically offset by -1.
        # Compute min/max to reverse the bias applied by C++
        u_vals = [args.uv[0], args.uv[2], args.uv[4], args.uv[6]]
        v_vals = [args.uv[1], args.uv[3], args.uv[5], args.uv[7]]
        
        max_u = max(u_vals)
        min_u = min(u_vals)
        max_v = max(v_vals)
        min_v = min(v_vals)
        
        def bias_rev(val, max_val, min_val):
            pix = round(val * 256.0)
            if abs(val - max_val) < 0.001 and max_val > min_val + 0.001:
                return int(pix - 1)
            return int(pix)
            
        pack["u0"] = bias_rev(args.uv[0], max_u, min_u)
        pack["v0"] = bias_rev(args.uv[1], max_v, min_v)
        pack["u1"] = bias_rev(args.uv[2], max_u, min_u)
        pack["v1"] = bias_rev(args.uv[3], max_v, min_v)
        pack["u2"] = bias_rev(args.uv[4], max_u, min_u)
        pack["v2"] = bias_rev(args.uv[5], max_v, min_v)
        pack["u3"] = bias_rev(args.uv[6], max_u, min_u)
        pack["v3"] = bias_rev(args.uv[7], max_v, min_v)

    with open(args.json_file, 'w') as f:
        json.dump(data, f, indent=4)
        
    print(f"Successfully patched face {args.face} in {args.json_file}")

if __name__ == "__main__":
    main()
