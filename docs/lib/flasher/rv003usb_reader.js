/* eslint-disable */
//
// ── rv003usb ブートローダから Flash を読み出す ─────────────────────────
//
// 「⚙ 準備」の「基板のファームを調べる」が使う。書き込みは一切しない。
//
// 下の payload と communicate_usb は、同梱の rv003usb_webflasher.js（MIT）から
// 読み出しに要るものだけを写したもの。フラッシャ本体は上流と差分を合わせるため
// 変更を 3 つに留めてあり、読み出しのための 4 つ目の変更は入れずに、ここへ分けた。
// ⚠ 写した部分は整形しない（上流と見比べられるように元の形のまま）。
//
// 元のフラッシャとの違い:
//   - Flash のロック解除（communicate_flash_unlock）をしない。読むだけなので不要と見ている。
//     ⚠ 実機では未確認。解除なしで読めなければ、ここを見直す。
//
/*
This WebFlasher is adapted from minichlink <https://github.com/cnlohr/ch32fun/tree/master/minichlink>
The adaptation was made by Sadale.

MIT License
Copyright (c) 2026 Wong Cho Ching <https://sadale.net>
Copyright (c) 2023-2024 CNLohr <lohr85@gmail.com>, et. al.
Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
Copyright (c) 2023-2024 E. Brombaugh
Copyright (c) 2023-2024 A. Mandera
Copyright (c) 2005-2020 Rich Felker, et al.
Copyright (c) 2013,2014 Michal Ludvig <michal@logix.cz>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

const FLASH_BASE = 0x08000000
export const FLASH_SIZE = 16384
const SECTOR_SIZE = 64

// ── ここから rv003usb_webflasher.js の写し ──

function dataview_to_uint8array(dataview) {
	let ret = new Uint8Array(dataview.byteLength);
	for(let i=0; i<dataview.byteLength; i++){
		ret[i] = dataview.getUint8(i)
	}
	return ret
}

function payload_obtain_uint8array(content) {
	const payload_size = 128;
	const blob_prefix = [0xaa, 0x00, 0x00, 0x00];
	const blob_suffix = [0xcd, 0xab, 0x34, 0x12];
	if(blob_prefix.length + blob_suffix.length + content.length > payload_size) {
		throw new Error("Payload content too long!");
	}

	let ret = new Uint8Array(payload_size);
	for(let i=0; i<blob_prefix.length; i++) {
		ret[i] = blob_prefix[i];
	}
	for(let i=0; i<blob_suffix.length; i++) {
		ret[ret.length-blob_suffix.length+i] = blob_suffix[i];
	}
	for(let i=0; i<content.length; i++) {
		ret[blob_prefix.length+i] = content[i]
	}
	return ret;
}

function uint32value_to_array(uint32value) {
	if(uint32value < 0 || uint32value > 2**32-1) {
		throw new Error("uint32value out of range");
	}
	let ret = []
	// Convert to little endian
	for(let i=0; i<4; i++) {
		ret.push(uint32value & 0xFF)
		uint32value /= 256
	}
	return ret
}

function bulid_halt_wait_payload() {
	return payload_obtain_uint8array([0x81, 0x46, 0x94, 0xc1, 0xfd, 0x56, 0x14, 0xc1, 0x82, 0x80])
}

function bulid_read_payload(address, size) {
	// blob_word_read, size and address must be aligned by 4.
	let payload = [
		0x23, 0xa0, 0x05, 0x00, 0x13, 0x07, 0x45, 0x03, 0x0c, 0x43, 0x50, 0x43,
		0x2e, 0x96, 0x21, 0x07, 0x94, 0x41, 0x14, 0xc3, 0x91, 0x05, 0x11, 0x07,
		0xe3, 0xcc, 0xc5, 0xfe, 0x93, 0x06, 0xf0, 0xff, 0x14, 0xc1, 0x82, 0x80,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
	payload = payload.concat(uint32value_to_array(address))
	payload = payload.concat(uint32value_to_array(size))
	return payload_obtain_uint8array(payload)
}

function bulid_run_app_payload() {
	// blob_run_app
	let payload = [
		0xb7,0xf5,0xff,0x1f,  // li     a1,0x1FFFF000   - load offset to a1
		0x93,0x87,0xc5,0x77,  // addi   a5,a1,0x77C     - load absolute address of secret area to a5
		0x03,0xa7,0x07,0x00,  // lw     a4,0(a5)        - load reboot function offset + xor from secret to a4
		0x13,0x57,0x07,0x01,  // srli   a4,a4,16        - shift it to remove lower part (offset)
		0x83,0x96,0x07,0x00,  // lh     a3,0(a5)        - load offset part to a3
		0x93,0xc7,0xc6,0x77,  // xori   a5,a3,0x77C     - find current xor
		0x63,0x16,0xf7,0x00,  // bne    a4,a5,.L2       - if xor is valid
		0x33,0x87,0xb6,0x00,  // add    a4, a3, a1      - make absolute address of reboot function an jump
		0x67,0x00,0x07,0x00,  // jr     a4              - jump to it
		/* else - means that we didn't find a reboot function address
		and need to send the blob to do a reboot
	.L2:                                                - Same sequence as in "Run app blob (old)"*/
		0xb7,0x27,0x02,0x40,  // li     a5,1073881088
		0x93,0x87,0x87,0x02,  // addi   a5,a5,40
		0x37,0x07,0x67,0x45,  // li     a4,1164378112
		0x13,0x07,0x37,0x12,  // addi   a4,a4,291
		0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
		0xb7,0x27,0x02,0x40,  // li     a5,1073881088
		0x93,0x87,0x87,0x02,  // addi   a5,a5,40
		0x37,0x97,0xef,0xcd,  // li     a4,-839938048
		0x13,0x07,0xb7,0x9a,  // addi   a4,a4,-1621
		0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
		0xb7,0x27,0x02,0x40,  // li     a5,1073881088
		0x93,0x87,0xc7,0x00,  // addi   a5,a5,12
		0x23,0xa0,0x07,0x00,  // sw     zero,0(a5)
		0xb7,0x27,0x02,0x40,  // li     a5,1073881088
		0x93,0x87,0x07,0x01,  // addi   a5,a5,16
		0x13,0x07,0x00,0x08,  // li     a4,128
		0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
		0xb7,0xf7,0x00,0xe0,  // li     a5,-536809472
		0x93,0x87,0x07,0xd1,  // addi   a5,a5,-752
		0x37,0x07,0x00,0x80,  // li     a4,-2147483648
		0x23,0xa0,0xe7,0x00,  // sw     a4,0(a5)
	]
	return payload_obtain_uint8array(payload)
}

async function communicate_usb(device, command, readback=true) {
	let retries = 0
	while(1) {
		try {
			await device.sendFeatureReport(command[0], command.slice(1));
			break
		} catch (error) {
			if(retries++ > 10) {
				console.error("sendFeatureReport retries exceeded!")
				return null
			}
		}
	}

	if(!readback) {
		return 0
	}

	retries = 0
	let timeout = 0
	let response = null
	while(1) {
		try {
			let dataview = await device.receiveFeatureReport(command[0])
			response = dataview_to_uint8array(dataview)
			if(response.byteLength == command.byteLength && response[1] == 0xff) {
				break;
			} else if(timeout++ > 20) {
				console.error("receiveFeatureReport timeout!")
				return null
			}
		} catch (error) {
			if(retries++ > 10) {
				console.error("receiveFeatureReport retries exceeded!")
				return null
			}
		}
	}
	return response;
}

// ── ここまで写し ──

/**
 * ブートローダ（1209:B803）の基板から Flash を全部読む。書き込みはしない。
 * 読み終えても書き込みモードのままにしておく。書き換えを勧めるときは、
 * そのまま「ファームウェアを書き込む」へ進めるように。戻すのは runApp()。
 *
 * @param {HIDDevice} device  開いていない状態で渡す
 * @param {(done:number, total:number) => void} [onProgress]
 * @returns {Promise<{ flash: Uint8Array }>}
 */
export async function readFlash(device, onProgress) {
	await device.open()
	try {
		if (await communicate_usb(device, bulid_halt_wait_payload()) === null) {
			throw new Error('基板と通信できませんでした（halt）')
		}
		const flash = new Uint8Array(FLASH_SIZE)
		for (let i = 0; i < FLASH_SIZE; i += SECTOR_SIZE) {
			const r = await communicate_usb(device, bulid_read_payload(FLASH_BASE + i, SECTOR_SIZE))
			if (r === null || r.length < 60 + SECTOR_SIZE) {
				throw new Error(`読み出しに失敗しました（0x${(FLASH_BASE + i).toString(16)}）`)
			}
			flash.set(r.subarray(60, 60 + SECTOR_SIZE), i)
			onProgress?.(i + SECTOR_SIZE, FLASH_SIZE)
		}
		return { flash }
	} finally {
		// 閉じておく。書き込むときはフラッシャが自分で開き直す
		try { await device.close() } catch (_) {}
	}
}

/**
 * 書き込みモードから普段の動作（通常のファームウェア）に戻す。
 * @param {HIDDevice} device  開いていない状態で渡す
 * @returns {Promise<boolean>} 戻す命令を送れたら true
 */
export async function runApp(device) {
	await device.open()
	try {
		return await communicate_usb(device, bulid_run_app_payload(), false) !== null
	} finally {
		// 普段の動作に戻ると、この device は消える。閉じられなくても構わない
		try { await device.close() } catch (_) {}
	}
}

/**
 * 読み出した Flash がどの配布ファームかを調べる。
 * 書き込みは bin の範囲しか書き換えないので、その後ろには前のファームの残りがある。
 * だから比べるのは各 bin の size バイトまで。
 *
 * @param {Uint8Array} flash
 * @param {Array<{binSha256:string, size:number}>} known
 * @returns {Promise<object|null>} 一致した known の要素。無ければ null
 */
export async function identifyFirmware(flash, known) {
	const shaBySize = new Map()
	for (const k of known) {
		if (k.size > flash.length) continue
		if (!shaBySize.has(k.size)) {
			const d = await crypto.subtle.digest('SHA-256', flash.subarray(0, k.size))
			shaBySize.set(k.size, [...new Uint8Array(d)].map(b => b.toString(16).padStart(2, '0')).join(''))
		}
		if (shaBySize.get(k.size) === k.binSha256) return k
	}
	return null
}
