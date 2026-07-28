"""Local-only driver: send Python to the running UnrealEditor via UE's built-in
Python Remote Execution (multicast discovery + TCP command channel).

Usage:  python ue_remote.py <editor_python_file>
The editor-side script should print() / unreal.log() its results; we relay them.
"""
import sys
import time

sys.path.append(r"D:\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as remote  # noqa: E402


def main():
    if len(sys.argv) < 2:
        print("usage: python ue_remote.py <editor_python_file>")
        sys.exit(1)
    with open(sys.argv[1], "r", encoding="utf-8") as fh:
        code = fh.read()

    conn = remote.RemoteExecution(remote.RemoteExecutionConfig())
    conn.start()
    node = None
    for _ in range(24):  # up to ~12s for discovery
        nodes = conn.remote_nodes
        if nodes:
            node = nodes[0]
            break
        time.sleep(0.5)
    if not node:
        print("NO_NODE: no Unreal remote-execution node discovered (is remote exec enabled + editor up?)")
        conn.stop()
        sys.exit(2)

    print("NODE: %s @ %s (%s)" % (node.get("user"), node.get("machine"), node.get("node_id")))
    conn.open_command_connection(node["node_id"])
    try:
        res = conn.run_command(
            code,
            unattended=True,
            exec_mode=remote.MODE_EXEC_FILE,
            raise_on_failure=False,
        )
    finally:
        conn.close_command_connection()
        conn.stop()

    for o in (res.get("output") or []):
        print("[%s] %s" % (o.get("type", "?"), o.get("output", "")))
    print("---")
    print("success:", res.get("success"))
    if res.get("result") not in (None, ""):
        print("result:", res.get("result"))


if __name__ == "__main__":
    main()
