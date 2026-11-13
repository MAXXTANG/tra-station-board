#!/usr/bin/env python3
"""Capture CRC-checked CYD framebuffer pages without changing saved settings."""
import argparse
from datetime import datetime
import hashlib
import html
import json
from pathlib import Path
import time
import zlib

from PIL import Image
import serial


class Board:
    def __init__(self, port):
        self.serial = serial.Serial()
        self.serial.port, self.serial.baudrate = port, 115200
        self.serial.timeout = .5
        self.serial.dtr = self.serial.rts = False
        self.serial.open()

    def line(self):
        return self.serial.readline().decode('utf-8', 'replace').strip()

    def state(self):
        self.serial.write(b'J')
        end = time.monotonic() + 4
        while time.monotonic() < end:
            line = self.line()
            if line.startswith('STATE '):
                return json.loads(line[6:])
        return None

    def navigate(self, request):
        self.serial.write(('V' + json.dumps(request, ensure_ascii=False) + '\n').encode())
        end = time.monotonic() + 5
        while time.monotonic() < end:
            line = self.line()
            if line.startswith('VIEW_OK '):
                return
            if line.startswith('VIEW_ERROR '):
                raise RuntimeError(line)
        raise RuntimeError('No navigation acknowledgement')

    def capture(self):
        deadline = time.monotonic() + 100
        while time.monotonic() < deadline:
            self.serial.write(b'S')
            end = time.monotonic() + 4
            busy = False
            while time.monotonic() < end:
                line = self.line()
                if line.startswith('CAPTURE_RLE565 '):
                    _, width, height, version = line.split()
                    assert version == '1'
                    return self.rows(int(width), int(height))
                if line == 'CAPTURE_BUSY':
                    busy = True
                    break
                if line.startswith('CAPTURE_REFUSED'):
                    raise RuntimeError(line + ': real setup credentials are never captured')
            if busy:
                time.sleep(.25)
        raise RuntimeError('Capture did not start')

    def rows(self, width, height):
        rgb = bytearray()
        expected, retries = 0, 0
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            line = self.line()
            if line == 'CAPTURE_ABORT':
                raise RuntimeError('Device aborted screenshot')
            if line == 'CAPTURE_END':
                assert expected == height and len(rgb) == width * height * 3
                return Image.frombytes('RGB', (width, height), bytes(rgb)), retries
            if not line.startswith('ROW '):
                continue
            try:
                _, index, encoded, checksum = line.split()
                index, packet = int(index), bytes.fromhex(encoded)
                if index != expected or zlib.crc32(packet) != int(checksum, 16):
                    raise ValueError('row index or CRC mismatch')
                if len(packet) % 3:
                    raise ValueError('invalid RLE packet')
                row = bytearray()
                for i in range(0, len(packet), 3):
                    count, hi, lo = packet[i:i+3]
                    if count == 0:
                        raise ValueError('zero-length run')
                    color = hi << 8 | lo
                    pixel = bytes(((color >> 11 & 31)*255//31,
                                   (color >> 5 & 63)*255//63,
                                   (color & 31)*255//31))
                    row.extend(pixel * count)
                if len(row) != width * 3:
                    raise ValueError('row width mismatch')
            except (ValueError, TypeError):
                self.serial.write(f'R{expected}\n'.encode())
                retries += 1
                continue
            rgb.extend(row)
            self.serial.write(f'A{expected}\n'.encode())
            expected += 1
        raise RuntimeError('Incomplete screenshot; no PNG was saved')


def screen_slug(request):
    view = request['view']
    if view == 'board':
        return f"board-direction-{request['direction']}"
    if view == 'regions':
        return f"regions-page-{request.get('page', 0)+1}"
    if view == 'stations':
        return f"county-stations-page-{request.get('page', 0)+1}"
    if view == 'calibrate_demo':
        return f"calibration-step-{request.get('step', 0)+1}"
    return {'recent':'recent-stations','switch_demo':'switch-station-demo','setup_demo':'wifi-setup-demo'}[view]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    stations = json.loads((root/'data/stations.json').read_text())
    regions = list(dict.fromkeys(s['region'] for s in stations if s['region']))
    board = Board(args.port)
    initial = None
    try:
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            initial = board.state()
            if initial and initial['wifi'] == 3 and not initial['portal'] and initial['calibrated'] and initial.get('clock_valid') and 0 <= time.time()-initial['updated'] < 60:
                break
            time.sleep(.5)
        else:
            raise RuntimeError('Board not ready: Wi-Fi, clock, timetable or touch calibration missing')
        selected = next(s for s in stations if s['id'] == initial['station'])
        county = selected['region']
        county_stations = [s for s in stations if s['region'] == county]
        left = initial.get('left_direction', 1)
        names = [initial.get('left_label', '北上'), initial.get('right_label', '南下')]
        entries = []

        def capture(folder, title, request, kind='實機 framebuffer；USB 診斷切頁'):
            number = len(entries) + 1
            board.navigate(request)
            image, retries = board.capture()
            path = args.output/folder/f'{number:02d}-{screen_slug(request)}.png'
            path.parent.mkdir(parents=True, exist_ok=True)
            image.resize((image.width*3, image.height*3), Image.Resampling.NEAREST).save(path)
            entries.append({'number':number,'title':title,'category':folder,'path':str(path.relative_to(args.output)),
                            'kind':kind,'request':request,'native_size':list(image.size),'crc_checked_rows':image.height,
                            'retries':retries,'captured_at':datetime.now().astimezone().isoformat(),
                            'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
            print(f'Captured {number:02d}: {title} ({retries} retries)', flush=True)
            return image

        first = capture('01-timetable', f'{selected["name"]}站・{names[0]}', {'view':'board','direction':left})
        capture('01-timetable', f'{selected["name"]}站・{names[1]}', {'view':'board','direction':1-left})
        capture('02-stations', '常用車站', {'view':'recent'})
        for page in range((len(regions)+7)//8):
            capture('02-stations', f'縣市清單 {page+1}/{(len(regions)+7)//8}', {'view':'regions','page':page})
        for page in range((len(county_stations)+7)//8):
            capture('02-stations', f'{county}車站 {page+1}/{(len(county_stations)+7)//8}', {'view':'stations','region':county,'page':page})
        capture('03-system', '切換車站提示', {'view':'switch_demo','station':'宜蘭' if selected['name']!='宜蘭' else '礁溪'},
                '實機渲染示例；沒有儲存選站或重啟')
        capture('03-system', '板上配網提示', {'view':'setup_demo'},
                '實機渲染示例；TRA-DEMO／DEMO1234 為假資料，未開啟熱點')
        for step in range(4):
            capture('04-calibration', f'觸控校正 {step+1}/4', {'view':'calibrate_demo','step':step},
                    '實機渲染校正提示；不改寫既有校正值')
        number = len(entries)+1
        path = args.output/'05-orientation'/f'{number:02d}-rotation-180.png'
        path.parent.mkdir(parents=True, exist_ok=True)
        first.transpose(Image.Transpose.ROTATE_180).resize((960,720),Image.Resampling.NEAREST).save(path)
        entries.append({'number':number,'title':'上下顛倒方向示意','category':'05-orientation',
                        'path':str(path.relative_to(args.output)),
                        'kind':'由第一張實機 framebuffer 旋轉示意；非實體方向／觸控驗收',
                        'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        board.navigate({'view':'restore'})
        after = board.state()
        assert after and not after['diagnostic']
        for key in ('station','direction','page','rotation','calibrated'):
            assert after[key] == initial[key], f'Initial state not restored: {key}'
        manifest = {'firmware':initial['version'],'station':initial['station'],'captured_on':datetime.now().astimezone().isoformat(),
                    'initial_state':initial,'restored_state':after,'entries':entries}
        (args.output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
        lines = ['# CYD 2.8 吋分類畫面','','原始 320×240，PNG 放大為 960×720。時刻表為擷取當下資料，非現在班次。',
                 '一般頁面以 USB 診斷切頁後讀取實機 framebuffer，每列經 CRC32 驗證。配網／切換／校正提示是實機渲染示例；旋轉圖另標示為示意。未上傳真實 Wi-Fi 密碼，擷取後已恢復原站點與畫面狀態。','']
        for category in dict.fromkeys(e['category'] for e in entries):
            titles={'01-timetable':'時刻表','02-stations':'選站介面','03-system':'系統提示','04-calibration':'觸控校正','05-orientation':'旋轉示意'}
            lines += ['## '+titles[category],'']
            for entry in entries:
                if entry['category'] != category:continue
                lines += [f'### {entry["number"]:02d} {entry["title"]}','',entry['kind'],'',f'![{entry["title"]}]({entry["path"]})','']
        (args.output/'README.md').write_text('\n'.join(lines))
        print('Restored device state; saved',len(entries),'categorized images',flush=True)
    finally:
        try:
            board.navigate({'view':'restore'})
        except Exception:
            pass
        board.serial.close()


if __name__ == '__main__':
    main()
