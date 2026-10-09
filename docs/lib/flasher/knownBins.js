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
    "binSha256": "367ed63276edc7b81cf2e9a21b13f76579aee40b6f578436edf6105f5e8b0fd7",
    "size": 15024,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / CAT24M01WI 0x50/0x51",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      }
    ]
  },
  {
    "binSha256": "00057e03305d67c804c7721d543932783b1ad319ffd49de0f79efe2f85686b59",
    "size": 15064,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / CAT24M01WI 0x50/0x51",
    "commits": [
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "dca4f21c9f8fb737d45758310ce1ef29f15f90bfbed1337a9a93708288c0e659",
    "size": 15088,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / EEPROM 自動認識 0x50",
    "commits": [
      {
        "hash": "15c2fa7",
        "date": "2026-10-02",
        "subject": "feat: URB Block Lab のファームで EEPROM の石を起動時に自動認識する"
      }
    ]
  },
  {
    "binSha256": "9e18710c6a11322d1cc8f92895a6d4fca81867c711b3f902f3e516080240c30b",
    "size": 15528,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / EEPROM 自動認識 0x50",
    "commits": [
      {
        "hash": "6cab51a",
        "date": "2026-10-02",
        "subject": "feat: URB Block Lab を 24FC256 でも動くようにする"
      }
    ]
  },
  {
    "binSha256": "4fed170e939cfd48d479663516dd2debc7f54bf9d05b0dfa813e60eda27c8ce9",
    "size": 15528,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / EEPROM 自動認識 0x50",
    "commits": [
      {
        "hash": "cb7ede5",
        "date": "2026-10-02",
        "subject": "feat: URB Block Lab で、基板のファームウェアが古いと知らせる"
      }
    ]
  },
  {
    "binSha256": "3525614edbb24cc01039ff78ab759e33473e89e0b75c73132565adc244fb8ae4",
    "size": 15536,
    "page": "URB Block Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNrUsRnEv",
    "label": "URB Block Lab 専用 / EEPROM 自動認識 0x50",
    "commits": []
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
  },
  {
    "binSha256": "74b12e83a6a8863b6178ac0f56ade51f4f8bb8bf0fa642f9cc16c8e736cf06b4",
    "size": 15888,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / 24FC256",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      }
    ]
  },
  {
    "binSha256": "f2a93b2f03ce623631fd77f012131cff28038192e95d122d254330fdb3496295",
    "size": 15900,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      }
    ]
  },
  {
    "binSha256": "ca03c2ee028c2a01625deadc5a8a7a8e275dc67a4f8792b109aeb2f0f7a8eb45",
    "size": 15900,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      }
    ]
  },
  {
    "binSha256": "b0351b2e61778bba75f258f2b76776301bb8c74f7b70bfd2b4c6b01cc4652a81",
    "size": 16340,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / 24FC256",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      },
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "63090b60a433506850adefc12c8504f72a678d1948f46ac6acf04cbf1ff97a91",
    "size": 16352,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      },
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "48d9ee757671d884913b74986c17ef19799cc4f83e51d5ed5cb38ef873ba868f",
    "size": 16352,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "7a0e767",
        "date": "2026-09-28",
        "subject": "fix: 配布ファームをコア 1.2.15 で作り直し、ピン 11 を IO として使えるようにする"
      },
      {
        "hash": "21bd37f",
        "date": "2026-09-28",
        "subject": "docs: 必要なボードパッケージを v1.2.15 以降に引き上げる"
      },
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "c0c55965df4d676af2f9e52c5993e28da8ac03b4611a1139b506b3b20e6aaf78",
    "size": 15924,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / 24FC256",
    "commits": [
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "23b07253490cef5771ba6b0416cec499b5c5ec163595de44dcaaaa9e906222f4",
    "size": 15936,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x50",
    "commits": [
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "0034ebc5486e4974a1f048b14fcbc73dcdbfc3973a48f03b902445da47fd244f",
    "size": 15936,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x52",
    "commits": [
      {
        "hash": "bd1a414",
        "date": "2026-09-28",
        "subject": "fix: PWM をピン 6 に出すと、ピン 12 に出てしまう不具合を直す"
      }
    ]
  },
  {
    "binSha256": "0a00dfa393eb62ba8689f35a3cf7dc58ecacaed824fae0185956c8b37c69df34",
    "size": 15936,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / 24FC256",
    "commits": []
  },
  {
    "binSha256": "a4e4a049f1366aa03fcf9e60c884f1e524288eafffc49fd2063114b112919e93",
    "size": 15948,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x50",
    "commits": []
  },
  {
    "binSha256": "2c1bf4292357c0e9e3bb07e4d2881fe827fd4700eba0ed27e21d1b8b16243c5e",
    "size": 15948,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1PwAdSeNpUsRnEc",
    "label": "サーボ / CAT24M01WI 0x52",
    "commits": []
  },
  {
    "binSha256": "88e64eeae33dbd357b994c7634820da49f197ff1aac117b1b8d383a2e6105147",
    "size": 16352,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / 24FC256",
    "commits": []
  },
  {
    "binSha256": "f5053fb3fb6cb5ceb92645477ae141a8a38c18c955fbe9a0577d54fb321bd5a4",
    "size": 16364,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x50",
    "commits": []
  },
  {
    "binSha256": "3971600c740febd1c51c76013c653ab51bc3793587283edbc80f961fb7f840f7",
    "size": 16364,
    "page": "URB EE Lab",
    "name": "UIAPrubyEeVmQ1TnAdSeNpUsRnEc",
    "label": "ブザー / CAT24M01WI 0x52",
    "commits": []
  }
];
