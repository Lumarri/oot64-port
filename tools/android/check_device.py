"""Check known renderer requirements against the connected phone's Vulkan report.

This is a necessary-capability check, not a rendering or gameplay test.
Requirements come from the pinned TriAevum vulkan_adapter_policy.cpp and
nri_interop_context.cpp. No device settings are changed.
"""
import argparse
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def collect(adb):
    result = subprocess.run(adb + ['shell', 'cmd', 'gpu', 'vkjson'], check=True,
                            capture_output=True, text=True, timeout=30)
    raw = json.loads(result.stdout)
    devices = []
    for device in raw.get('devices', []):
        properties = device['properties']
        version = int(properties['apiVersion'])
        major, minor, patch = version >> 22 & 0x7F, version >> 12 & 0x3FF, version & 0xFFF
        extensions = {item['extensionName'] for item in device.get('extensions', [])}
        blockers = []
        if (major, minor) < (1, 2):
            blockers.append('Pinned TriAevum adapter policy requires Vulkan >= 1.2')
        # This revision explicitly negotiates the extension, even on core 1.3.
        if 'VK_KHR_synchronization2' not in extensions:
            blockers.append('Pinned NRI interop requires VK_KHR_synchronization2')
        devices.append({'gpu': properties['deviceName'], 'vulkan': f'{major}.{minor}.{patch}',
                        'create_renderpass2': 'VK_KHR_create_renderpass2' in extensions,
                        'synchronization2': 'VK_KHR_synchronization2' in extensions,
                        'dynamic_rendering': 'VK_KHR_dynamic_rendering' in extensions,
                        'blockers': blockers})
    return {'scope': 'necessary_capabilities_only_not_full_renderer_qualification',
            'devices': devices,
            'known_requirements_met': any(not item['blockers'] for item in devices)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial')
    args = parser.parse_args()
    adb = ['adb', '-s', args.serial] if args.serial else ['adb', '-d']
    report = collect(adb)
    config = json.loads((ROOT / 'config/android-base.json').read_text())
    report['base_commit'] = config['commit']
    destination = ROOT / 'local/android-logs/renderer-preflight.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['known_requirements_met'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
