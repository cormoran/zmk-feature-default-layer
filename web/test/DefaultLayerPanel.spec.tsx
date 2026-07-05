import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import {
  createConnectedMockZMKApp,
  ZMKAppProvider,
} from "@cormoran/zmk-studio-react-hook/testing";
import { DefaultLayerPanel, SUBSYSTEM_IDENTIFIER } from "../src/App";
import {
  Response,
  StateResponse,
} from "../src/proto/cormoran/default-layer/default_layer";

jest.mock("@zmkfirmware/zmk-studio-ts-client", () => ({
  create_rpc_connection: jest.fn(),
  call_rpc: jest.fn(),
}));

import { call_rpc } from "@zmkfirmware/zmk-studio-ts-client";

function mockRpcResponse(state: StateResponse) {
  const payload = Response.encode(Response.create({ state })).finish();
  (call_rpc as jest.Mock).mockResolvedValueOnce({
    custom: { call: { payload } },
  });
}

const sampleState: StateResponse = {
  endpoints: [
    { index: 1, isUsb: true, bleProfileIndex: 0, value: -1 },
    { index: 2, isUsb: false, bleProfileIndex: 0, value: 2 },
  ],
  osLayers: [
    { os: 0, value: -1 },
    { os: 1, value: 1 },
    { os: 2, value: -1 },
    { os: 3, value: -1 },
    { os: 4, value: -1 },
    { os: 5, value: -1 },
  ],
  activeEndpointIndex: 2,
  currentOs: 1,
  resolvedLayer: 2,
  layerCount: 4,
  osDetectionAvailable: true,
};

describe("DefaultLayerPanel", () => {
  beforeEach(() => {
    (call_rpc as jest.Mock).mockReset();
  });

  describe("Without subsystem", () => {
    it("shows a warning when the subsystem is not found", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DefaultLayerPanel />
        </ZMKAppProvider>
      );

      expect(
        screen.getByText(
          new RegExp(`Subsystem "${SUBSYSTEM_IDENTIFIER}" not found`, "i")
        )
      ).toBeInTheDocument();
    });
  });

  describe("With subsystem", () => {
    it("fetches and renders endpoint and OS state", async () => {
      mockRpcResponse(sampleState);
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DefaultLayerPanel />
        </ZMKAppProvider>
      );

      await waitFor(() => {
        expect(screen.getByText(/Current OS:/i)).toBeInTheDocument();
      });

      expect(screen.getByText(/Current OS:/i)).toHaveTextContent("Windows");
      expect(screen.getByText(/Current OS:/i)).toHaveTextContent(
        "Resolved layer: 2"
      );
      expect(screen.getByText("USB")).toBeInTheDocument();
      expect(screen.getByText(/BLE profile 0 \(active\)/)).toBeInTheDocument();

      const usbSelect = screen.getByLabelText(
        "USB default layer"
      ) as HTMLSelectElement;
      expect(usbSelect.value).toBe("-1");

      const bleSelect = screen.getByLabelText(
        "BLE profile 0 default layer"
      ) as HTMLSelectElement;
      expect(bleSelect.value).toBe("2");

      const windowsSelect = screen.getByLabelText(
        "Windows default layer"
      ) as HTMLSelectElement;
      expect(windowsSelect.value).toBe("1");

      expect(screen.getByLabelText("iOS default layer")).toBeInTheDocument();
      expect(
        screen.getByLabelText("Android default layer")
      ).toBeInTheDocument();
    });

    it("sends a set_endpoint_layer request when a connection's layer changes", async () => {
      mockRpcResponse(sampleState);
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DefaultLayerPanel />
        </ZMKAppProvider>
      );

      const usbSelect = await screen.findByLabelText("USB default layer");

      mockRpcResponse({
        ...sampleState,
        endpoints: [
          { index: 1, isUsb: true, bleProfileIndex: 0, value: 0 },
          sampleState.endpoints[1],
        ],
      });

      const user = userEvent.setup();
      await user.selectOptions(usbSelect, "Layer 0");

      await waitFor(() => {
        expect((usbSelect as HTMLSelectElement).value).toBe("0");
      });

      expect(call_rpc).toHaveBeenCalledTimes(2);
    });

    it("disables the per-OS table when OS detection is unavailable", async () => {
      mockRpcResponse({ ...sampleState, osDetectionAvailable: false });
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DefaultLayerPanel />
        </ZMKAppProvider>
      );

      const windowsSelect = (await screen.findByLabelText(
        "Windows default layer"
      )) as HTMLSelectElement;
      expect(windowsSelect.disabled).toBe(true);
      expect(
        screen.getByText(/OS detection is not enabled in this firmware build/i)
      ).toBeInTheDocument();
    });
  });
});
