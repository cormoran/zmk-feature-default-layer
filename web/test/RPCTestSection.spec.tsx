import { render, screen } from "@testing-library/react";
import {
  createConnectedMockZMKApp,
  ZMKAppProvider,
} from "@cormoran/zmk-studio-react-hook/testing";
import { DefaultLayerPanel, SUBSYSTEM_IDENTIFIER } from "../src/App";

describe("DefaultLayerPanel Component", () => {
  describe("With Subsystem", () => {
    it("should render controls when subsystem is found", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DefaultLayerPanel />
        </ZMKAppProvider>
      );

      expect(screen.getByText(/Transport Layer Map/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/Layer/i)).toBeInTheDocument();
      expect(screen.getByText(/Refresh/i)).toBeInTheDocument();
    });

    it("should show default layer input value", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <DefaultLayerPanel />
        </ZMKAppProvider>
      );

      const input = screen.getByLabelText(/Layer/i) as HTMLInputElement;
      expect(input.value).toBe("0");
    });
  });

  describe("Without Subsystem", () => {
    it("should show warning when subsystem is not found", () => {
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
        screen.getByText(/Subsystem "zmk__default_layer" not found/i)
      ).toBeInTheDocument();
    });
  });

  describe("Without ZMKAppContext", () => {
    it("should not render when ZMKAppContext is not provided", () => {
      const { container } = render(<DefaultLayerPanel />);

      expect(container.firstChild).toBeNull();
    });
  });
});
