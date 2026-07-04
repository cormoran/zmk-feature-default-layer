import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { setupZMKMocks } from "@cormoran/zmk-studio-react-hook/testing";
import App, { SUBSYSTEM_IDENTIFIER } from "../src/App";
import { Response } from "../src/proto/cormoran/default-layer/default_layer";

// Mock the ZMK client
jest.mock("@zmkfirmware/zmk-studio-ts-client", () => ({
  create_rpc_connection: jest.fn(),
  call_rpc: jest.fn(),
}));

jest.mock("@zmkfirmware/zmk-studio-ts-client/transport/serial", () => ({
  connect: jest.fn(),
}));

describe("App Component", () => {
  describe("Basic Rendering", () => {
    it("should render the application header", () => {
      render(<App />);

      expect(screen.getByText(/ZMK Default Layer/i)).toBeInTheDocument();
      expect(
        screen.getByText(
          /Per-connection and per-OS default layer configuration/i
        )
      ).toBeInTheDocument();
    });

    it("should render connection button when disconnected", () => {
      render(<App />);

      expect(screen.getByText(/Connect Serial/i)).toBeInTheDocument();
    });

    it("should render footer", () => {
      render(<App />);

      expect(
        screen.getByText(/Per-endpoint and per-OS default layer/i)
      ).toBeInTheDocument();
    });
  });

  describe("Connection Flow", () => {
    let mocks: ReturnType<typeof setupZMKMocks>;

    beforeEach(() => {
      mocks = setupZMKMocks();
      const payload = Response.encode(
        Response.create({
          state: {
            endpoints: [],
            osLayers: [],
            activeEndpointIndex: 0,
            currentOs: 0,
            resolvedLayer: 0,
            layerCount: 1,
            osDetectionAvailable: false,
          },
        })
      ).finish();
      (mocks.call_rpc as jest.Mock).mockResolvedValue({
        custom: { call: { payload } },
      });
    });

    it("should connect to device when connect button is clicked", async () => {
      mocks.mockSuccessfulConnection({
        deviceName: "Test Keyboard",
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      const { connect: serial_connect } =
        await import("@zmkfirmware/zmk-studio-ts-client/transport/serial");
      (serial_connect as jest.Mock).mockResolvedValue(mocks.mockTransport);

      render(<App />);

      expect(screen.getByText(/Connect Serial/i)).toBeInTheDocument();

      const user = userEvent.setup();
      const connectButton = screen.getByText(/Connect Serial/i);
      await user.click(connectButton);

      await waitFor(() => {
        expect(
          screen.getByText(/Connected to: Test Keyboard/i)
        ).toBeInTheDocument();
      });

      expect(screen.getByText(/Disconnect/i)).toBeInTheDocument();
      expect(
        screen.getByRole("heading", { level: 2, name: /Default Layer/i })
      ).toBeInTheDocument();
    });
  });
});
