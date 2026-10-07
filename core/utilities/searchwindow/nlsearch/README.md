# Interface the database search engine to an AI-based LLM

A user types a description such as *"landscape photos with red labels"*; the pipeline
maps that text to a structured intent, converts it to digiKam's Search XML, and
populates the existing Advanced Search widgets. The LLM produces what
a human clicking the dialog could produce.

## Pipeline overview

```
User query text
      │
SearchQueryEngine  ──(check)──>  SearchQueryCache        (hit: return early)
      │
SearchPromptBuilder            (system rules + JSON schema + collection hints)
      │
SearchLanguageBackend          (mock now / llama later, async, off GUI thread)
      │
SearchIntentParser             (extract JSON, whitelist-validate every field/op)
      │
SearchIntentResolver  <──────>  SearchCapabilityDictionary   (aliases, values)
      │
emits exactly one of:
   signalIntentReady              > populate Advanced Search via Search XML
   signalClarificationRequired    > ask the user (e.g. "best" is ambiguous)
   signalErrorOccurred            > explain; leave the dialog untouched
```

## Classes

### `SearchQueryIntent` / `SearchQueryConstraint`  (`searchqueryintent.h`)

Value types that carry a parsed query through the pipeline.

`SearchQueryConstraint` is one constraint: `field` (canonical name such as
`tag`, `daterange`, `colorlabel`), `op` (`eq`, `contains`, `gte`, `lte`,
`between`), and `value`. `isValid()` reports whether field and op are set.

`SearchQueryIntent` is the full result: the list of constraints, clarification
state (`requiresClarification`, `clarificationMessage`, `clarificationChoices`),
the verbatim `originalQuery` and the `normalizedQuery` used as the cache key,
the raw model output for debugging, and `parseSucceeded`. Helpers: `isEmpty()`,
`isAmbiguous()`. Both types are registered with `Q_DECLARE_METATYPE` so they
travel through queued signal/slot connections.

### `SearchLanguageBackend`  (`searchlanguagebackend.h/.cpp`)

Abstract `QObject` interface that isolates the pipeline from any specific model
runtime.

- `loadModel(path)` / `unloadModel()` / `isModelLoaded()` - model lifecycle.
- `slotRunInference(prompt)` *(slot)* - must return immediately; the result arrives
  later via signal. This contract is what keeps the GUI responsive.
- `cancel()` *(slot)* - best-effort cancellation.
- Signals: `signalRawOutputReady(QString)`, `signalInferenceError(QString)`,
  `signalModelLoaded()`, `signalInferenceProgress(...)`.

### `SearchMockBackend`  (`searchmockbackend.h/.cpp`)

Canned-response backend implementing the abstract interface. Lets the entire
pipeline run and be unit-tested without a real model, and remains useful for CI
permanently.

- `addCannedResponse(querySubstring, json)` - register a fixed output.
- `slotRunInference(prompt)` - substring-matches the prompt and emits the matching
  JSON via `QTimer::singleShot(0, …)`, so the asynchronous signal ordering is
  identical to a real backend. An unmatched prompt yields a valid-but-empty
  schema, which exercises the engine's "nothing understood" path.
- The constructor pre-loads the proposal's demonstration queries.

### `SearchLlamaBackend`  (`searchllamabackend.h/.cpp`)  *(optional)*

llama.cpp / GGUF backend, compiled only when `ENABLE_NLSEARCH_LLAMACPP=ON`
(off by default, so master builds stay clean). The front-end object is a thin
signal router; a `SearchLlamaWorker` lives on a `QThread` and owns the model and
context as opaque handles so `llama.h` never leaks into other headers.

- `loadModel` / `slotRunInference` / `cancel` forward to the worker via queued
  signals; `setMaxTokens(int)` and `setTemperature(float)` set generation params
  (temperature 0 = greedy, deterministic - desirable for parsed output).
- The token decode loop is the integration point (TODO).

### `SearchPromptBuilder`  (`searchpromptbuilder.h/.cpp`)

Builds the constrained prompt sent to the backend.

- `buildPrompt(query, knownTags, knownAlbums, knownPeople)` - assembles the
  system rules, the JSON output schema, and collection hints (capped per list so
  the prompt stays within small-model context windows), followed by the user's
  request.
- `schemaDescription()` / `systemPrompt()` - the supported fields, operators,
  ISO date format, and the "respond only with JSON, never invent metadata"
  instructions.

### `SearchIntentParser`  (`searchintentparser.h/.cpp`)

Turns raw model output into a validated `SearchQueryIntent`. Treats model output
as untrusted.

- `parse(rawOutput, original, normalized)` - extracts the first balanced JSON
  object (tolerating prose around it), parses it, and builds the intent. Any
  unknown field or operator rejects the **whole** output (`parseSucceeded =
  false`) rather than applying part of it.
- `validate(intent)` / `validationErrors(intent)` - whitelist checks; range
  operators are restricted to the fields that support them.

### `SearchCapabilityDictionary`  (`searchcapabilitydictionary.h/.cpp`)

The maintainable map between natural-language terms and digiKam capabilities.
Extending search support means editing this table, not retraining a model.

- `initializeDefaults()` - field aliases (e.g. *"location"* → `place`),
  enumerated allowed values (color labels, pick labels, orientation), value
  aliases, and ambiguous-word entries with their clarification choices.
- `resolveFieldAlias(alias, out)` / `resolveValue(field, raw, out)` - map a term
  to its canonical form; free-form fields pass through, enumerated fields must
  match.
- `choicesForAmbiguousWord(word)` - clarification options for words like
  *"best"*.
- `setKnownTags(...)` / `isKnownTag(...)` - collection awareness.

### `SearchIntentResolver`  (`searchintentresolver.h/.cpp`)

Turns a parsed intent into concrete, applicable criteria, deciding what can be
applied and what must be surfaced to the user.

- `resolve(intent)` → `ResolvedSearchCriteria` - resolves each constraint
  through the dictionary; anything unresolvable goes into `unresolvedTerms`
  (shown to the user, never silently dropped) rather than being invented.
- `canApplyDirectly(intent)` - true when there are results and no clarification
  is pending.

### `SearchQueryCache`  (`searchquerycache.h/.cpp`)

In-memory cache so repeated queries skip inference.

- `lookup(query, out)` / `store(query, intent)` - keyed on the trimmed,
  lower-cased, simplified query; only successful, non-ambiguous intents are
  cached. Bounded (256 entries).
- `clear()` / `size()` / `invalidateSchemaVersion(v)` - maintenance; the last
  lets persisted caches expire when the prompt schema changes.

### `SearchNlModelManager`  (`searchnlmodelmanager.h/.cpp`)  *(skeleton)*

Model-file lifecycle. Bridges to digiKam's `DNNModelManager` and
`FilesDownloader` (the same mechanism the face/auto-tag models use) so the
backend simply receives a local path.

- `defaultModelPath()` / `isModelAvailable()` *(static)* - path conventions.
- `ensureModelAvailable()` - emits `signalModelReady` if present.
- Signals: `signalDownloadProgress`, `signalModelReady`, `signalModelError`.

### `SearchQueryEngine`  (`searchqueryengine.h/.cpp`)

The coordinator - the only class that knows the pipeline order.

- `setBackend / setPromptBuilder / setParser / setResolver / setCache` -
  dependency injection (the unit test wires the same engine with the mock;
  `SearchWindow` wires it with production parts). `setBackend` safely re-wires
  signal connections.
- `setKnownTags / setKnownAlbums / setKnownPeople` - prompt hints.
- `slotInterpretQuery(original, normalized)` *(slot)* - the entry point. Busy-guard,
  cache check, prompt build, then `slotRunInference`. An empty `normalized` falls
  back to `original` (the translation-failure contract). Cache hits are emitted
  asynchronously so timing matches the inference path.
- `slotRawOutputReady(output)` - parse → resolve → emit one of `signalIntentReady`,
  `signalClarificationRequired`, or `signalErrorOccurred`; reports skipped terms via
  `signalStatusMessage` and caches successful results.
- Signals: `signalIntentReady`, `signalClarificationRequired`, `signalErrorOccurred`,
  `signalStatusMessage`.

---

## Building and testing

The pipeline builds into digiKam by default; no extra flags are required.

### Unit tests

```sh
cmake -DBUILD_TESTING=ON ..
make -j$(nproc) nlsearchpipeline_utest
ctest -R nlsearch --output-on-failure
```
---

## 1. Architecture

Two concrete backends implement the abstract `SearchLanguageBackend`:

| Backend               | Purpose                                             | Model required |
|-----------------------|-----------------------------------------------------|----------------|
| `SearchMockBackend`   | Canned responses; default build, pipeline dev/tests | No             |
| `SearchLlamaBackend`  | Real GGUF inference via llama.cpp on a worker thread| Yes            |

`SearchLlamaBackend` runs all `llama_*` calls on a dedicated `QThread`
(`SearchLlamaWorker`), so model loading and inference never block the GUI.
Results are delivered back through queued signals.

The default model is **Qwen2.5-1.5B-Instruct** (Q4_K_M, ~1 GB, Apache-2.0).
Decoding is greedy (temperature 0) because the output must be reproducible,
parseable JSON rather than creative text.

---

## 2. Enabling the llama.cpp backend

The backend is **OFF by default**. A normal build uses only the mock backend
and links nothing from llama.cpp. To build the real backend:

    cmake .. -DENABLE_NLSEARCH_LLAMACPP=ON \
             -DCMAKE_BUILD_TYPE=Release \
             -DOpenCV_DIR=/opt/opencv-4.8/lib/cmake/opencv4

### 2.1 Vendored llama.cpp

llama.cpp is **vendored** (copied in-tree) under
`core/utilities/searchwindow/thirdparty/llama.cpp`, not pulled as a git
submodule: KDE's repository hooks reject `.gitmodules`, so the sources are
committed directly. There is nothing to initialise — a normal checkout already
contains it.

It is built **library-only** and **CPU-only** (no CUDA/Metal/Vulkan). The ggml
compute engine is bundled inside llama.cpp; there is no external ggml
dependency. The GPU backend sources are left in the tree but not compiled.

All of llama.cpp's build options are set by a small digiKam-owned wrapper,
`thirdparty/CMakeLists.txt`, which is the single place to look when adjusting
the vendored build (see §3).

### 2.2 Build type and inference speed

**Important:** llama.cpp / ggml are roughly 30x slower when compiled without
optimization. A plain `Debug` build makes a single query take tens of seconds.

To keep developers able to build the rest of digiKam in Debug while still
getting usable inference, the bundled llama/ggml targets are forced to compile
with optimization even in Debug builds (see the CMake notes below). End users
should still build with `-DCMAKE_BUILD_TYPE=Release`.

---

## 3. How llama.cpp was integrated (CMake)

Adding a git submodule and building it in-tree touched several places. Each
change is small and documented here so the integration can be reviewed and
maintained.

### 3.1 Vendoring and updating llama.cpp

The sources under `thirdparty/llama.cpp/` are a trimmed copy of upstream
llama.cpp. To update to a newer upstream version:

  1. Replace the tree with the new upstream sources.
  2. Re-check the wrapper options in `thirdparty/CMakeLists.txt` still match
     upstream's option names (they occasionally rename `LLAMA_*` / `GGML_*`).
  3. Commit **all** files the build needs, including CMake includes such as
     `cmake/build-info.cmake`. Upstream's own `.gitignore` or a partial copy
     can leave such files untracked; a file present on your disk but not
     committed builds locally yet fails a fresh checkout with
     `include could not find requested file`. After copying, verify with a
     clean configure in an empty build directory.

No `.gitmodules` is involved; the vendored copy is committed like any other
source.

### 3.2 `core/CMakeLists.txt`

  - Sets `HAVE_LLAMACPP` when `ENABLE_NLSEARCH_LLAMACPP` is ON, **before**
    `digikam_config.h` is generated, so the flag reaches the compiler:

        if(ENABLE_NLSEARCH_LLAMACPP)
            set(HAVE_LLAMACPP TRUE)
        endif()

  - The old `find_package(Llama)` / `MACRO_BOOL_TO_01(Llama_FOUND ...)` lines
    were removed; the submodule is always present when the option is on, so
    there is nothing to "find".

### 3.3 `core/app/utils/digikam_config.h.cmake.in`

  - Added `#cmakedefine HAVE_LLAMACPP 1` so the CMake variable becomes a real
    compiler define.

### 3.4 The wrapper: `thirdparty/CMakeLists.txt`

All of llama.cpp's build configuration lives in this wrapper, so the vendored
`CMakeLists.txt` is never edited. It:

  - disables everything digiKam does not need — demo apps, tools, tests,
    server, examples, common utilities, and network/CURL support;
  - builds **CPU-only** (`GGML_CUDA/METAL/VULKAN OFF`);
  - uses a **portable CPU baseline** (`GGML_NATIVE OFF`): `-march=native` would
    make a distributed AppImage crash with SIGILL on older CPUs;
  - builds a **static** library (`BUILD_SHARED_LIBS OFF`), which exports no
    symbols and so avoids the Windows DLL symbol-export problem;
  - wraps `add_subdirectory(llama.cpp)` between
    `set(CMAKE_SKIP_INSTALL_RULES ON/OFF)` so llama.cpp's own install rules do
    not leak its library, headers, or CMake config into digiKam's install
    prefix;
  - runs `find_package(OpenMP)` (non-APPLE) **before** the subdirectory, so
    ggml's own OpenMP probe finds it and parallelises inference instead of
    silently serialising;
  - forces `-O2` on the `llama` / `ggml*` targets in Debug builds only, since
    ggml at `-O0` is ~30× slower and makes a single query take tens of seconds,
    while leaving the rest of digiKam debuggable.

`core/utilities/searchwindow/CMakeLists.txt` then simply does
`add_subdirectory(thirdparty)` when `HAVE_LLAMACPP` is set.

### 3.5 Linking into `digikamgui`

Because the wrapper builds `llama` as a normal in-tree static target,
`digikamgui` links it by target name, guarded by `HAVE_LLAMACPP`:

    if(HAVE_LLAMACPP)
        target_link_libraries(digikamgui PRIVATE llama)
        target_include_directories(digikamgui PRIVATE
            ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/thirdparty/llama.cpp/include)
    endif()

`libllama` carries its own ggml dependency, so ggml need not be linked
explicitly. The include directory exposes llama's public headers to the
backend sources.

### 3.6 `core/cmake/macros/MacroUtils.cmake`

  - `HEADER_DIRECTORIES()` recursively globs `*.h` and feeds every directory
    into the global include path. With llama.cpp now living inside
    `core/utilities`, that swept the submodule's headers into unrelated targets
    (dimg, facesengine, ...) and broke their compilation. Fixed by excluding
    vendored code from the sweep:

        list(FILTER new_list EXCLUDE REGEX "/thirdparty/")

    This is a latent issue that any future vendored code would also have hit.

---

## 4. Code changes (summary)

  - `searchllamabackend.{h,cpp}` : real model loading (`slotDoLoad`), the
    decode loop (`slotDoInference`) with greedy sampling and an early stop once
    a balanced JSON object is produced, and cleanup (`slotDoUnload`).
  - `searchwindow.cpp` : selects the llama backend when the feature is built
    and the model is installed, otherwise falls back to the mock backend.
  - `searchintentparser.cpp` : handles JSON values that arrive as numbers or
    booleans, not only strings (a numeric rating was previously dropped).
  - `searchpromptbuilder.cpp` : enforces `start..end` date ranges, injects the
    current date so relative dates ("last year") resolve correctly, and
    strengthens the instruction to abstain rather than guess.
  - `searchqueryengine.cpp` : a clearer "model not installed" vs. "model still
    loading" message.

---

## 5. Testing

### 5.1 Unit tests (no model required)

The pipeline is covered by
`core/tests/nlsearch/nlsearchpipeline_utest.cpp`, which runs entirely against
the mock backend:

    cd <build-dir>
    ctest -R nlsearchpipeline_utest --output-on-failure

It covers composite queries, clarification, error-not-guess behaviour, cache
hits, malformed-output rejection, and two regressions found during
development: numeric JSON values and date-range preservation.

### 5.2 Manual verification of real inference

With the backend enabled and the model installed:

  1. Build with `-DENABLE_NLSEARCH_LLAMACPP=ON -DCMAKE_BUILD_TYPE=Release`.
  2. Launch digiKam; open Advanced Search (or the sidebar search field).
  3. Type a request, e.g. *"photos from 2023 rated 5 stars"*, and press Enter.
  4. The Advanced Search criteria populate (date range 2023-01-01..2023-12-31,
     rating 5) and the matching photos are returned.

Model load takes a few seconds on first use; querying before it finishes shows
the "still loading" message.

---

## 6. Getting the model

The Qwen2.5-1.5B model is hosted on the KDE mirrors and downloaded through
digiKam's standard model download dialog: tick **"Use Natural Language Search
feature"**, download, and the file is SHA-256 verified and placed in the shared
model directory. **Restart digiKam** after downloading.

If the model is not installed, the feature degrades gracefully to the mock
backend and the search field reports that the model needs to be downloaded.

## Further reading

Development notes and design rationale from the GSoC 2026 project:

- [Teaching digiKam to Understand You: Natural Language Search with Local LLMs](https://srirupa19.github.io/gsoc/2026/06/28/gsoc1.html) —
  the architecture, why the LLM only translates rather than searches, and why
  Qwen2.5-1.5B-Instruct.

- [The Model Was Never the Hard Part: Integrating Qwen2.5 into digiKam for Natural Language Search](https://srirupa19.github.io/gsoc/2026/07/14/gsoc2.html) —
  vendoring and statically linking llama.cpp, the CMake wrapper that isolates
  its build, and why integration was the hard part rather than the model.

## Adding a new search criterion

Supporting a new field end-to-end touches four places, in pipeline order:

1. **Teach the model** - `searchpromptbuilder.cpp`, `schemaDescription()`:
   add the field to the "Supported fields" list and, importantly, add a worked
   example using it. Small models rely on examples far more than on prose
   rules; without one the model tends to grab an unrelated field.

2. **Accept it** - `searchintentparser.cpp`, `supportedFields()`: add the
   canonical field name to the whitelist, or the parser rejects the output.

3. **Map its terms** - `searchcapabilitydictionary.cpp`, `initializeDefaults()`:
   register natural-language aliases for the field and any enumerated values,
   and (if the word is ambiguous) an entry in `ambiguousWords` with the
   clarification choices.

4. **Emit the XML** - `searchwindow.cpp`, `writeConstraintToXml()`: add a
   branch that turns the resolved constraint into digiKam Search XML, using the
   correct field name from `searchfields_createfield.cpp`. Values the model
   lower-cases (tags, people, places) must be resolved against the collection's
   stored values case-insensitively, since the stored spelling is what the
   database matches.

Then add a canned response to `SearchMockBackend` and a case to the pipeline
unit test so the new field is covered without a real model.
