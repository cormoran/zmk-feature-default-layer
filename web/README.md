# zmk-feature-default-layer - Web Frontend

Web UI for configuring `zmk-feature-default-layer` over the unofficial custom
ZMK Studio RPC protocol: pick a default layer per connection (USB / each BLE
profile), or set it to "OS detection" and let it follow a per-OS mapping
resolved by [zmk-feature-os-detection](https://github.com/cormoran/zmk-feature-os-detection).

## Quick Start

```bash
# Install dependencies
npm install

# Generate TypeScript types from proto
npm run generate

# Run development server
npm run dev

# Build for production
npm run build

# Run tests
npm test
```

## Project Structure

```
src/
├── main.tsx              # React entry point
├── App.tsx               # Connection UI + DefaultLayerPanel
├── App.css               # Styles
└── proto/                # Generated protobuf TypeScript types
    └── cormoran/default-layer/
        └── default_layer.ts

test/
├── App.spec.tsx                # Tests for App component / connection flow
└── DefaultLayerPanel.spec.tsx  # Tests for the endpoint/OS layer panel
```

## How It Works

### 1. Protocol Definition

The protobuf schema is defined in `../proto/cormoran/default-layer/default_layer.proto`.
`value` fields share one sentinel encoding: `>=0` is a keymap layer index,
`-1` means unset, and `-2` (endpoint values only) means "resolve via the
per-OS mapping".

### 2. Code Generation

TypeScript types are generated using `ts-proto`:

```bash
npm run generate
```

This runs `buf generate` which uses the configuration in `buf.gen.yaml`.

### 3. Using react-zmk-studio

```typescript
import { useZMKApp, ZMKCustomSubsystem } from "@cormoran/zmk-studio-react-hook";

const { state, connect, findSubsystem, isConnected } = useZMKApp();
const subsystem = findSubsystem("cormoran__default_layer");
const service = new ZMKCustomSubsystem(state.connection, subsystem.index);
const response = await service.callRPC(payload);
```

## Testing

```bash
# Run all tests
npm test

# Run tests in watch mode
npm run test:watch

# Run tests with coverage
npm run test:coverage
```
