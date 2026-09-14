// このファイルは tools/gen-known-bins.mjs が作ります。手で編集しないでください。
//
// これまでに配ったファームの一覧（git の履歴にある blockBin.js / sketchBins.js の全版）。
// 「⚙ 準備」の「基板のファームを調べる」が、基板から読み出した Flash の先頭 size バイトの
// SHA-256 をここの binSha256 と比べて、どの版が入っているかを出す。
//
// commits が空のものは、まだコミットしていないビルド。

export const KNOWN_BINS = [
  {
    "binSha256": "823a6cd5491763b8aa51293357e3bf539442f3d67dd17ed424efa910e820fad6",
    "size": 15004,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / CAT24M01WI 0x52/0x53",
    "commits": [
      {
        "hash": "81e22ca",
        "date": "2026-09-03",
        "subject": "feat: URB Block Lab を追加する"
      }
    ]
  },
  {
    "binSha256": "e29db68086245ac6312f3da4d53c7d8c7305a22bb9b8cd2219bf009d39bc1fbc",
    "size": 15004,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / CAT24M01WI 0x50/0x51",
    "commits": [
      {
        "hash": "0894b61",
        "date": "2026-09-03",
        "subject": "fix: 同梱ファームを CAT24M01WI 0x50/0x51 版に戻す"
      }
    ]
  },
  {
    "binSha256": "e309fb2463158f6d96668ec7e7bdd41f6d1fe0ea4d160367cffd8ecb13e30d25",
    "size": 15004,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / CAT24M01WI 0x50/0x51",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "2b807103b25149ec1ee2d04555809931f8a38fe39685965216e1198d7eb4c59a",
    "size": 15004,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / CAT24M01WI 0x50/0x51",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  },
  {
    "binSha256": "3748d108ca843bb6cd8015adde97072e46cfe0beebd55a2d4c28833ac057f07a",
    "size": 15864,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / 24FC256",
    "commits": [
      {
        "hash": "9a0361c",
        "date": "2026-08-18",
        "subject": "feat: ビルド済みファームをブラウザから直接書き込めるようにする"
      },
      {
        "hash": "1889dbb",
        "date": "2026-08-25",
        "subject": "feat: 絶対周期コンポーネント Tm を両 Lab に追加し、CAT24M01WI 0x52 対応を取り込む"
      }
    ]
  },
  {
    "binSha256": "54e926c5220de28b8db61f802fb8eaeb850fc5a28d08dd2f07f93089cb2a2364",
    "size": 15876,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "9a0361c",
        "date": "2026-08-18",
        "subject": "feat: ビルド済みファームをブラウザから直接書き込めるようにする"
      },
      {
        "hash": "1889dbb",
        "date": "2026-08-25",
        "subject": "feat: 絶対周期コンポーネント Tm を両 Lab に追加し、CAT24M01WI 0x52 対応を取り込む"
      }
    ]
  },
  {
    "binSha256": "3ba8780840a8cda0d934f0264e40ba5cd3424d9e5bdef7537cd717552aba3ee4",
    "size": 16320,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / 24FC256",
    "commits": [
      {
        "hash": "9a0361c",
        "date": "2026-08-18",
        "subject": "feat: ビルド済みファームをブラウザから直接書き込めるようにする"
      },
      {
        "hash": "1889dbb",
        "date": "2026-08-25",
        "subject": "feat: 絶対周期コンポーネント Tm を両 Lab に追加し、CAT24M01WI 0x52 対応を取り込む"
      }
    ]
  },
  {
    "binSha256": "fb4555c8006ec80b0a35b25b7c6a59ff56faff8c5628c2cdc2d32d34d08d7c05",
    "size": 16332,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "9a0361c",
        "date": "2026-08-18",
        "subject": "feat: ビルド済みファームをブラウザから直接書き込めるようにする"
      },
      {
        "hash": "1889dbb",
        "date": "2026-08-25",
        "subject": "feat: 絶対周期コンポーネント Tm を両 Lab に追加し、CAT24M01WI 0x52 対応を取り込む"
      }
    ]
  },
  {
    "binSha256": "6271b9d128c53656d9d91e4a6f000be7a1ce59d58fd08b92f96aa7233277ba9d",
    "size": 15876,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "1889dbb",
        "date": "2026-08-25",
        "subject": "feat: 絶対周期コンポーネント Tm を両 Lab に追加し、CAT24M01WI 0x52 対応を取り込む"
      }
    ]
  },
  {
    "binSha256": "1223f3a5b7a882eeeab0fb59872aac41604fb489c0dd42aa5bc65a4e6c541782",
    "size": 16332,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "1889dbb",
        "date": "2026-08-25",
        "subject": "feat: 絶対周期コンポーネント Tm を両 Lab に追加し、CAT24M01WI 0x52 対応を取り込む"
      }
    ]
  },
  {
    "binSha256": "522a2717d6104b63315df8c09ff75ca3254ebd9499e5ef307d96732891ded7fb",
    "size": 15864,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / 24FC256",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "f1f6282200c6af5434d4cf64df089b548f71892a663baeaf6712032a4935658e",
    "size": 15876,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "1c3749e8afdb55ce5748a99dcf839f0b44837aaf0c164bbf7108e93659ade9b1",
    "size": 15876,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "00549b393ecbce6b78f61602ed33b79dca0d8553748ef54e60def2eb7e90ea4e",
    "size": 16320,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / 24FC256",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "1940eb886f78cf478acc58d23ffadfb6141e15fceddf7df2bcb2c700f54e7087",
    "size": 16332,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "50d68d894d4a280514061e4f2464188c36e231babeeae6a4cb2dc04d0b5fa753",
    "size": 16332,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "2763378",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab を本運用にし、起動待ちを 2.5 秒に短くする"
      }
    ]
  },
  {
    "binSha256": "3a85aa63769d56432bea7e0b07250b2bc184cd4231b93f58d1c8bced149bd75e",
    "size": 15864,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / 24FC256",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  },
  {
    "binSha256": "b86d29e80136731d15bfeb75bdcf6e6fb31fc08cda655562e040c48836e4e77b",
    "size": 15876,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  },
  {
    "binSha256": "13ade5ba12015e89bb329733d9d6ed357e357e38b25c2d525110d38a09e61f89",
    "size": 15876,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  },
  {
    "binSha256": "c1063c46367cff5c2eb129a4fff937e057bc16bc3d00dd88125f4c5347ba4947",
    "size": 16320,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / 24FC256",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  },
  {
    "binSha256": "5953de443eec43bf2f51ab2b04cb9385bea7977a0561d1be88264daad6ec1bef",
    "size": 16332,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  },
  {
    "binSha256": "a4533020afd2747d43fac29cf49bea9c0d3c4f96d7fa9fcec549eb86d50fe1bc",
    "size": 16332,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "88c606b",
        "date": "2026-09-14",
        "subject": "feat: URB Block Lab で基板のファームウェアを調べられるようにし、起動待ちを 1 秒にする"
      }
    ]
  }
];
