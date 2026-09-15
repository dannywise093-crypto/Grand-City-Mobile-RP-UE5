# Perbaikan push GitHub dari Fork

## Penyebab

Commit lokal `4d513a887d851890648ad92160095b338b029377` ikut memasukkan hasil
build Unreal dan cache editor karena repository belum mempunyai `.gitignore`.
Fork melaporkan HTTP 408, RPC failed, dan remote hang up. Pemeriksaan langsung
menunjukkan branch `main` GitHub masih pada
`42432ae8c244ead0883399df3eb6b4824bb3db50`: push tersebut belum diterima.

Dari 488 file pada commit, 442 merupakan file generated yang perlu dikeluarkan
dari pelacakan:

| Kelompok | File | Ukuran isi sebelum kompresi |
| --- | --- | --- |
| `Intermediate/` | 365 | 7.22 GiB |
| `Binaries/` | 14 | 755.94 MiB |
| `.vs/` | 10 | 90.82 MiB |
| `Saved/` | 43 | 6.60 MiB |
| `DerivedDataCache/` | 6 | 1.80 MiB |
| Solution `.sln`/`.slnx` di root | 4 | Generated project files |

Contoh file yang ikut masuk: tiga precompiled header `.pch` berukuran sekitar
2.0–2.4 GiB per file, executable game 316.49 MiB, dan PDB 348.72 MiB.
GitHub memblokir file Git biasa di atas 100 MiB. Ukuran push juga dibatasi 2 GB.
Ukuran transfer terkompresi tidak sama dengan jumlah ukuran file mentah, tetapi
file individual pada commit ini sudah melampaui batas.
Referensi: [batas file GitHub](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github)
dan [batas repository/push](https://docs.github.com/en/repositories/creating-and-managing-repositories/repository-limits).

HTTP 408 menunjukkan request timeout. Log Fork saja tidak menentukan komponen
network yang mengirim timeout, tetapi isi commit yang terlalu besar terbukti
dan harus diperbaiki agar push dapat diterima. Pesan `Everything up-to-date`
di akhir dialog tidak membuktikan commit sudah masuk; hash remote adalah
pemeriksaan yang menentukan.

## Perbaikan

`.gitignore` sekarang mengabaikan output build, cache, solution generated,
dependency Node, dan file environment lokal backend. Source, Config, Content,
Docs, descriptor project, `.vsconfig`, serta source/schema/config backend tetap
dilacak. `Build/` dan binary vendor plugin tidak diabaikan secara menyeluruh karena
dapat berisi resource penting.

Commit lokal yang belum berhasil dipush dibersihkan dengan `git rm --cached`
dan amend. Cara ini mempertahankan file di disk. Source/perubahan migrasi pada
commit lama dipertahankan. Membuat commit penghapusan baru saja tidak cukup,
karena blob besar tetap berada dalam commit sebelumnya yang ikut dikirim.
Ini juga merupakan [prosedur GitHub untuk file besar pada commit terakhir yang belum dipush](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github#removing-a-file-added-in-the-most-recent-unpushed-commit).

Commit lama dipertahankan pada ref lokal
`refs/codex/backups/ue58-before-push-cleanup`. Ref ini bukan branch/tag dan tidak
ikut push normal `main`. Karena backup masih menunjuk blob lama, ukuran folder
`.git` lokal belum langsung mengecil; hal ini tidak berarti blob lama ikut push
branch yang sudah dibersihkan.

Push dilakukan sebagai fast-forward biasa ke `origin/main`; tidak memerlukan
force push karena commit besar belum berada di remote. Sesudah push, hash HEAD
lokal dibandingkan dengan `refs/heads/main` pada GitHub.

## Menggunakan Fork setelah perbaikan

Refresh repository di Fork. Commit terbaru mempunyai hash baru karena amend.
File di `Binaries/`, `Intermediate/`, `Saved/`, `.vs/`, dan `DerivedDataCache/`
tetap tersedia untuk editor/build lokal, tetapi tidak muncul lagi sebagai Local
Changes. Commit selanjutnya cukup memasukkan source, config, aset, dan dokumen
yang memang dibutuhkan project.

Untuk membagikan executable hasil build, gunakan release/artifact distribution.
Jika aset project besar nantinya melebihi batas Git biasa, atur Git LFS untuk
jenis aset tersebut sebelum commit. Cache build tidak perlu disimpan lewat LFS.
