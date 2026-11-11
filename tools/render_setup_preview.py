"""Render the firmware's setup form as an offline, non-saving preview."""
from pathlib import Path
import html
import json
import re

root = Path(__file__).resolve().parents[1]
source = (root / 'include/portal.h').read_text()
parts = dict(re.findall(r'(PORTAL_\w+)\[\] = R"HTML\((.*?)\)HTML";', source, re.S))
stations = json.loads((root / 'data/stations.json').read_text())
options = ''.join(
    f'<option value="{html.escape(s["id"], quote=True)}" data-region="{html.escape(s["region"], quote=True)}"'
    + (' selected' if s['id'] == '7210' else '')
    + f'>{html.escape(s["name"])} · {s["id"]}</option>' for s in stations
)
current = {'labels': 'ns', 'flip': '0', 'rotation': '0', 'auto': False}
page = (parts['PORTAL_HEAD']
        + '<label>Wi-Fi 名稱</label><input name="ssid" required maxlength="32">'
        + parts['PORTAL_MIDDLE'] + options + '</select>'
        + '<script type="application/json" id="current">' + json.dumps(current) + '</script>'
        + parts['PORTAL_TAIL'])
page = page.replace('<form method="post" action="/save">',
    '<p style="color:#e2bc65">設定頁預覽：請在實機熱點中儲存。</p>'
    '<form onsubmit="event.preventDefault();alert(\'這是離線預覽，請連接實機熱點後儲存。\')">')
(root / 'setup-preview.html').write_text(page)
print(f'Setup preview generated with {len(stations)} stations and screen orientation controls')
