# Third-party notices

Instrument Hub is distributed under the GNU Affero General Public License
v3.0. The components below remain under their own licences.

## JUCE 8

- Project: https://juce.com/
- Source: `third_party/JUCE` (git submodule)
- Licence: dual-licensed under AGPLv3 or the commercial JUCE licence.
  This project uses the AGPLv3 option.
- Full text: [`third_party/JUCE/LICENSE.md`](third_party/JUCE/LICENSE.md)

JUCE bundles additional third-party components (including the VST3 SDK,
HarfBuzz, SheenBidi and platform SDK headers). Their notices and licence files
are kept inside the JUCE submodule. In particular:

- VST3 SDK: `third_party/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`
- JUCE module-specific notices: alongside each module under
  `third_party/JUCE/modules/`

## cpp-httplib

- Project: https://github.com/yhirose/cpp-httplib
- Source: `third_party/cpp-httplib`
- Licence: MIT
- Copyright (c) 2017 yhirose
- Full text: [`third_party/cpp-httplib/LICENSE`](third_party/cpp-httplib/LICENSE)

## Bundled JavaScript runtime

The production UI bundle contains the following MIT-licensed packages:

| Package | Copyright |
|---|---|
| `react` | Meta Platforms, Inc. and affiliates |
| `react-dom` | Meta Platforms, Inc. and affiliates |
| `scheduler` | Meta Platforms, Inc. and affiliates |
| `openapi-fetch` | openapi-ts contributors |
| `openapi-typescript-helpers` | openapi-ts contributors |

Build-only tools (Vite, TypeScript, Prettier, `@vitejs/plugin-react` and
`openapi-typescript`) are not shipped in the application bundle.

### MIT licence text

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
