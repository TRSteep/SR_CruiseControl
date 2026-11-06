# Third-party dependencies

- **Dear ImGui 1.91.6**: source in `imgui-1.91.6`, [upstream](https://github.com/ocornut/imgui/tree/v1.91.6), license in `imgui-1.91.6/LICENSE.txt`.
- **MinHook 1.3.4**: source in `minhook-1.3.4`, [upstream](https://github.com/TsudaKageyu/minhook/tree/v1.3.4), license in `minhook-1.3.4/LICENSE.txt`.
- **Ultimate ASI Loader 10.0.2 x64**: [upstream source and release](https://github.com/ThirteenAG/Ultimate-ASI-Loader/tree/v10.0.2), license in `asi-loader-v10.0.2/LICENSE.txt`. The binary is downloaded by `prepare-dependencies.ps1` and is not stored in Git.

Upstream source archives used for the vendored trees:

| Archive | SHA256 |
| --- | --- |
| `https://github.com/ocornut/imgui/archive/refs/tags/v1.91.6.zip` | `549963A124817C03514CEB9D3F49D4A5A4C10BDC747FDC62D3797B9B2030135F` |
| `https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.4.zip` | `172708123DAA0C98D20D3A980B16A50BE14AF243DC95DEE6F79C24193AD010E4` |
| `Ultimate-ASI-Loader-NoPDB_x64.zip`, release `v10.0.2` | `FE964ADF4DE7F467FF39C440A4059DE02E37D0574A62944EEF2F55A35EDAFA2F` |

Precompiled example libraries, downloaded archives and build products are excluded from Git. Project builds use the vendored source files rather than those example libraries.
