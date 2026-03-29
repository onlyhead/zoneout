# Changelog

## [0.0.7] - 2026-03-29

### <!-- 0 -->⛰️  Features

- Add support for local coordinate mode and nodes
- Add JSON serialization for Workspace
- Introduce Workspace for holistic management
- Add Geoget for interactive geographic data input

## [0.0.6] - 2026-02-20

### <!-- 2 -->🚜 Refactor

- Replace geoson/geotiv with vectkit/rastkit

## [0.0.5] - 2026-01-18

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Refactor: Migrate build system from xmake to cmake
- Migrate project metadata to PROJECT file
- Refactor build system and update project metadata

## [0.0.4] - 2026-01-16

### <!-- 0 -->⛰️  Features

- Refactor Zone and Plot APIs for consistency
- Improve build system flexibility and plot handling

### <!-- 1 -->🐛 Bug Fixes

- Re-enable and update file I/O round-trip test

## [0.0.3] - 2026-01-01

### <!-- 0 -->⛰️  Features

- Refactor Zoneout to header-only library

## [0.0.2] - 2025-12-31

### <!-- 1 -->🐛 Bug Fixes

- Update test files for new datapod/concord API

### <!-- 2 -->🚜 Refactor

- Consolidate build system logic and project dependencies
- Migrate test files from concord to datapod types
- Migrate all examples to datapod/dp types
- Migrate zoneout to new robolibs architecture
- Update to robolibs architecture pattern

## [1.4.0] - 2025-12-15

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Update devbox dependencies
- Refactor: Update devbox dependencies
- Refactor build system and update dependencies
- Feat: Add xmake build system support

## [1.3.0] - 2025-12-15

### <!-- 0 -->⛰️  Features

- Introduce C++ Builder pattern for plot and zone objects
- Introduce fluent builders for zone and plot objects
- Introduce spdlog and add multi-resolution zones example

### <!-- 2 -->🚜 Refactor

- Move PlotBuilder and ZoneBuilder into their base classes

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Refactor build system and update dependencies
- Cleanup accidentally comminted files

## [1.2.0] - 2025-11-16

### <!-- 0 -->⛰️  Features

- Add grid center example
- Introduce I/O module and zone creation helpers
- Add C++ quickstart for zoneout library

### <!-- 2 -->🚜 Refactor

- Remove 3D occlusion layer and standardize naming
- Standardize API to snake_case and remove Layer class
- Refactor: Remove occlusion layer support from Zone
- Introduce metadata struct to core geometric types
- Encapsulate `Zone`'s internal data with accessors
- Organize source files and add future tasks

### <!-- 3 -->📚 Documentation

- Refactor: Remove outdated refactoring summary documentation
- Streamline project documentation and quickstart experience
- Remove TODO file and ignore it from now on

### <!-- 5 -->🎨 Styling

- Rename C++ methods to snake_case for consistency

## [1.1.0] - 2025-11-02

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Update geoson and devbox packages

## [1.0.0] - 2025-11-01

### <!-- 2 -->🚜 Refactor

- Refactor class structure and update CMake build system

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Refactor build system and migrate class method definitions
- Update dependencies and remove unused ones

### Build

- Introduce project reconfiguration and dependency updates

## [0.3.3] - 2025-10-31

### <!-- 0 -->⛰️  Features

- Introduce spatially-referenced data layer

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Update dependency versions for concord and geotiv
- Update `geotiv` to version 2.0.2

## [0.3.2] - 2025-08-08

### <!-- 6 -->🧪 Testing

- Enhance grid and polygon robustness for varied real-world data

## [0.3.1] - 2025-08-08

### <!-- 0 -->⛰️  Features

- Add Zone object (de)serialization

## [0.2.6] - 2025-08-08

### <!-- 1 -->🐛 Bug Fixes

- Reverse Y-axis for GIS/image coordinates

## [0.2.5] - 2025-08-04

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Clean up build system dependencies

## [0.2.4] - 2025-08-04

### <!-- 0 -->⛰️  Features

- Implement core ISOXML generation and plot persistence

### <!-- 2 -->🚜 Refactor

- Refactor field creation and improve zone visualization
- Adopt Rerun for enhanced visualization capabilities
- Refactor plot loading and zone management

### <!-- 3 -->📚 Documentation

- Relocate documentation examples and remove unused tests

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Update GEOJSON and CRS handling

## [0.2.1] - 2025-07-30

### <!-- 0 -->⛰️  Features

- Update geotiff processing and documentation

### <!-- 2 -->🚜 Refactor

- Disable function `record_api_data` call

### <!-- 3 -->📚 Documentation

- Add plotting classes and examples

## [0.2.0] - 2025-07-09

### <!-- 0 -->⛰️  Features

- Refactor Plot to handle tar file saving and loading
- Add plot management system and I/O capabilities
- Add `microtar` archive processing library

## [0.1.0] - 2025-07-08

### <!-- 0 -->⛰️  Features

- Refactor polygon creation and initialization logic
- Refactor grid generation using AABB and add debug info
- Refactor and enhance geospatial processing with improved polygon tools

### <!-- 2 -->🚜 Refactor

- Refactor zone management and integrate polygon features

### <!-- 4 -->⚡ Performance

- Use robust point-in-polygon instead of `indices_within`

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Remove pigment as an external dependency

## [0.0.4] - 2025-07-03

### <!-- 0 -->⛰️  Features

- Enhance zone generation and raster data handling

## [0.0.3] - 2025-06-29

### <!-- 2 -->🚜 Refactor

- Refactor Zone to manage raster data through an initial grid
- Refactor: improve boundary handling in structured elements

## [0.0.2] - 2025-06-29

### <!-- 0 -->⛰️  Features

- Unify spatial data handling and grid generation
- Refactor element handling and introduce structured types
- Introduce poly and grid geometry management classes
- Refactor zone property management for global consistency
- Improve file I/O for `Zone` and documentation of `Farm`
- Revamp zone and farm management with spatial data types
- Introduce `Zone` class for agricultural area management
- Introduce UUID and time utilities
- Init

### <!-- 2 -->🚜 Refactor

- Harden `Zone` class to use composition over inheritance
- Refactor `Zone` for direct construction and external raster use
- Refactor API for improved consistency and generalization
- Refactor: Introduce Zone class for better organization

### <!-- 3 -->📚 Documentation

- Refactor documentation into a comprehensive user guide
- Update project description

