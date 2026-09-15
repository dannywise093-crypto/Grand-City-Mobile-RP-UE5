# Perbaikan project Unreal Engine 5.8

Project ini menggunakan Unreal Engine **5.8.2** yang terpasang di
`E:\APLIKASI\UE_5.8`. Asosiasi `5.8` pada `GrandCityMobile.uproject` sudah sesuai.

## Akar masalah dan perbaikan

| Masalah | Perbaikan |
| --- | --- |
| `delegate void` bukan deklarasi callback C++ yang valid. Ini menyebabkan error sintaks, tipe callback tidak dikenali, lambda gagal, dan pemanggilan fungsi dianggap memiliki jumlah argumen salah. | Empat tipe callback menggunakan `TFunction<void(...)>`, sesuai pemanggilan dengan lambda yang sudah ada. |
| `DOREPLIFETIME` mengacu ke variabel lokal bernama `OutLifetimeProps`, tetapi implementasi GameState dan PlayerState memakai `OutLifetimeReplicatedProps`. | Nama parameter dan pemanggilan `Super::GetLifetimeReplicatedProps` disamakan menjadi `OutLifetimeProps`. |
| `FGenericPlatformHttp::UrlEncode` digunakan tanpa header yang mendeklarasikannya. | Tambahkan `GenericPlatform/GenericPlatformHttp.h`. |
| Dua `.Target.cs` berada di root project, sehingga UnrealBuildTool tidak menemukan target project dan sebelumnya memakai target engine `UnrealEditor`. | Pindahkan keduanya ke `Source/`, sehingga target `GrandCityMobile` dan `GrandCityMobileEditor` dikenali. |
| Target masih memakai pengaturan build/include order untuk versi sebelumnya. | Gunakan `BuildSettingsVersion.V7` dan `EngineIncludeOrderVersion.Unreal5_8`. |
| Dependensi tipe tertentu hanya tersedia lewat include tidak langsung. | Tambahkan header `TimerManager`, `World`, `JsonWriter`, `StaticMesh`, dan `SceneComponent` pada file yang memakai tipe tersebut. |
| Variabel lokal dalam lambda pemuatan profil menyembunyikan variabel fungsi induk. | Ganti nama variabel dalam lambda agar kompatibel dengan pemeriksaan compiler Unreal. |
| Linking gagal dengan `LNK2019` pada `FUniqueNetIdWrapper::ToString`. | Tambahkan dependensi private `CoreOnline` pada module rules dan header online pada GameMode. |
| Startup melaporkan `DirectoryWatcher` gagal memantau folder `Content/` yang belum ada. | Sediakan folder `Content/` dengan `.gitkeep`. |

Deklarasi callback yang salah, masalah nama parameter replikasi, header HTTP yang
hilang, dan lokasi target juga ditemukan di source project asli 5.6 pada
`D:\Git_Clone\Grand-City-Mobile-RP-UE5`. Temuan ini berdasarkan perbandingan source;
build dengan engine 5.6 belum diuji.

Referensi Epic: [replikasi properti dan `DOREPLIFETIME`](https://dev.epicgames.com/documentation/unreal-engine/replicate-actor-properties-in-unreal-engine)
serta [`FUniqueNetIdWrapper` pada modul CoreOnline](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreOnline/FUniqueNetIdWrapper).

## Build ulang

Jalankan dari root project di PowerShell setelah instalasi Visual Studio/C++ selesai:

```powershell
$UnrealRoot = 'E:\APLIKASI\UE_5.8'
$ProjectFile = Join-Path (Get-Location) 'GrandCityMobile.uproject'

& "$UnrealRoot\Engine\Build\BatchFiles\Build.bat" GrandCityMobileEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE -NoEngineChanges
& "$UnrealRoot\Engine\Build\BatchFiles\Build.bat" GrandCityMobile Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE -NoEngineChanges
```

Target Editor diperlukan untuk membuka project. Target Game memeriksa kompilasi
kode runtime Windows; ini belum merupakan packaging Windows atau Android.

## Hasil verifikasi

Verifikasi dilakukan pada 15 September 2026, menggunakan UE 5.8.2, MSVC
14.44.35228 dari folder toolchain 14.44.35207, dan Windows SDK 10.0.26100.0.

| Pemeriksaan | Hasil | Log |
| --- | --- | --- |
| `GrandCityMobileEditor Win64 Development` | Succeeded; seluruh source dikompilasi dan DLL ditautkan. | `Saved/Logs/UE58-Editor-Build.log` |
| `GrandCityMobile Win64 Development` | Succeeded; executable runtime Windows ditautkan. | `Saved/Logs/UE58-Game-Build.log` |
| Regenerasi solution Visual Studio | Succeeded; kedua target ditemukan pada `Source/`. | `Saved/Logs/UE58-ProjectFiles.log` |
| Startup editor unattended dengan NullRHI | DLL project dimuat, inisialisasi engine selesai, lalu keluar dengan kode 0; tidak ada log Error/Fatal. | `Saved/Logs/UE58-Startup-Final.log` |

Perintah uji startup:

```powershell
& "$UnrealRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectFile" -NullRHI -unattended -NoSplash -NoSound -NoP4 -ExecCmds=QUIT_EDITOR
```

Uji ini memeriksa startup dan pemuatan modul tanpa rendering GPU, serta tidak
menjalankan Play. Warning folder `Content/` tidak muncul pada uji final. Warning
layout editor versi lama ditangani otomatis oleh Unreal. Konfigurasi project yang
ditulis otomatis saat uji editor dikembalikan ke nilai sebelum uji.

## Batas pemeriksaan

Folder asli 5.6 belum berisi `Content/`, `.umap`, atau `.uasset`. Folder `Content/`
telah disediakan pada project 5.8, tetapi belum berisi aset map atau Blueprint.
Karena itu, aset hasil konversi belum dapat diperiksa. Tidak ada
plugin Unreal aktif pada `Plugins/GrandCityOpenAI`; folder tersebut berisi dokumen.

Pemuatan profil dari backend wajib berhasil sebelum GameMode memunculkan karakter.
Untuk menguji Play, jalankan backend sesuai `Docs/DURABLE_PERSISTENCE.md` dan
samakan konfigurasi API key. Kegagalan koneksi backend berbeda dari error kompilasi.

Pemeriksaan AutoSDK saat startup menunjukkan SDK Android `r27c` belum valid.
Build/packaging Android belum diverifikasi; kedua build yang lulus adalah Windows.

Saat verifikasi, update Visual Studio sempat membuat file compiler tidak tersedia,
sehingga build awal tertahan sebelum kompilasi C++. Setelah compiler dan library
x64 tersedia kembali, kedua target berhasil dibangun seperti dicatat di atas.
