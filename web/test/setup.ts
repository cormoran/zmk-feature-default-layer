// jest-dom adds custom jest matchers for asserting on DOM nodes.
import "@testing-library/jest-dom";

// jsdom's built-in TextEncoder/TextDecoder aren't real constructors, which
// breaks @bufbuild/protobuf's BinaryWriter. Use Node's implementations
// instead - needed by any test that actually encodes/decodes a proto message.
import { TextEncoder, TextDecoder } from "node:util";
globalThis.TextEncoder = TextEncoder as typeof globalThis.TextEncoder;
globalThis.TextDecoder = TextDecoder as typeof globalThis.TextDecoder;
