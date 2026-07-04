import { useCallback, useContext, useEffect, useState } from "react";
import "./App.css";
import { connect as serial_connect } from "@zmkfirmware/zmk-studio-ts-client/transport/serial";
import {
  ZMKConnection,
  ZMKCustomSubsystem,
  ZMKAppContext,
} from "@cormoran/zmk-studio-react-hook";
import {
  Request,
  Response,
  StateResponse,
} from "./proto/cormoran/default-layer/default_layer";

export const SUBSYSTEM_IDENTIFIER = "cormoran__default_layer";

const UNSET = -1;
const OS_DETECTION = -2;

const OS_LABELS = ["Unknown", "Windows", "macOS", "Linux"];

function App() {
  return (
    <div className="app">
      <header className="app-header">
        <h1>🗂️ ZMK Default Layer</h1>
        <p>Per-connection and per-OS default layer configuration</p>
      </header>

      <ZMKConnection
        renderDisconnected={({ connect, isLoading, error }) => (
          <section className="card">
            <h2>Device Connection</h2>
            {isLoading && <p>⏳ Connecting...</p>}
            {error && (
              <div className="error-message">
                <p>🚨 {error}</p>
              </div>
            )}
            {!isLoading && (
              <button
                className="btn btn-primary"
                onClick={() => connect(serial_connect)}
              >
                🔌 Connect Serial
              </button>
            )}
          </section>
        )}
        renderConnected={({ disconnect, deviceName }) => (
          <>
            <section className="card">
              <h2>Device Connection</h2>
              <div className="device-info">
                <h3>✅ Connected to: {deviceName}</h3>
              </div>
              <button className="btn btn-secondary" onClick={disconnect}>
                Disconnect
              </button>
            </section>

            <DefaultLayerPanel />
          </>
        )}
      />

      <footer className="app-footer">
        <p>Per-endpoint and per-OS default layer for ZMK</p>
      </footer>
    </div>
  );
}

function layerValueOptions(layerCount: number, allowOsDetection: boolean) {
  const options: { value: number; label: string }[] = [
    { value: UNSET, label: "— (unset)" },
  ];
  if (allowOsDetection) {
    options.push({ value: OS_DETECTION, label: "OS detection" });
  }
  for (let i = 0; i < layerCount; i++) {
    options.push({ value: i, label: `Layer ${i}` });
  }
  return options;
}

function endpointLabel(endpoint: { isUsb: boolean; bleProfileIndex: number }) {
  return endpoint.isUsb ? "USB" : `BLE profile ${endpoint.bleProfileIndex}`;
}

export function DefaultLayerPanel() {
  const zmkApp = useContext(ZMKAppContext);
  const [state, setState] = useState<StateResponse | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const subsystem = zmkApp?.findSubsystem(SUBSYSTEM_IDENTIFIER);

  const callRPC = useCallback(
    async (request: Request): Promise<StateResponse | null> => {
      if (!zmkApp?.state.connection || !subsystem) return null;

      setIsLoading(true);
      setError(null);
      try {
        const service = new ZMKCustomSubsystem(
          zmkApp.state.connection,
          subsystem.index
        );
        const payload = Request.encode(request).finish();
        const responsePayload = await service.callRPC(payload);
        if (!responsePayload) return null;

        const resp = Response.decode(responsePayload);
        if (resp.error) {
          setError(resp.error.message);
          return null;
        }
        if (resp.state) {
          setState(resp.state);
          return resp.state;
        }
        return null;
      } catch (err) {
        setError(err instanceof Error ? err.message : "Unknown error");
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [zmkApp, subsystem]
  );

  const refresh = useCallback(() => {
    void callRPC(Request.create({ getState: {} }));
  }, [callRPC]);

  useEffect(() => {
    // refresh()'s setState calls happen after the RPC round-trip (async),
    // not synchronously in the effect body - the lint rule can't see through
    // the useCallback/async indirection.
    // eslint-disable-next-line react-hooks/set-state-in-effect
    refresh();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [subsystem?.index]);

  if (!zmkApp) return null;

  if (!subsystem) {
    return (
      <section className="card">
        <div className="warning-message">
          <p>
            ⚠️ Subsystem "{SUBSYSTEM_IDENTIFIER}" not found. Make sure your
            firmware includes the default-layer module.
          </p>
        </div>
      </section>
    );
  }

  const setEndpointLayer = (endpointIndex: number, value: number) =>
    callRPC(Request.create({ setEndpointLayer: { endpointIndex, value } }));

  const setOsLayer = (os: number, value: number) =>
    callRPC(Request.create({ setOsLayer: { os, value } }));

  return (
    <section className="card">
      <h2>Default Layer</h2>

      {error && (
        <div className="error-message">
          <p>🚨 {error}</p>
        </div>
      )}

      {!state ? (
        <p>{isLoading ? "⏳ Loading..." : "No data yet."}</p>
      ) : (
        <>
          <div className="status-line">
            <p>
              Current OS: <strong>{OS_LABELS[state.currentOs] ?? "?"}</strong>
              {" · "}
              Resolved layer: <strong>{state.resolvedLayer}</strong>
            </p>
          </div>

          <h3>Connections</h3>
          <table className="endpoint-table">
            <thead>
              <tr>
                <th>Connection</th>
                <th>Default layer</th>
              </tr>
            </thead>
            <tbody>
              {state.endpoints.map((endpoint) => (
                <tr
                  key={endpoint.index}
                  className={
                    endpoint.index === state.activeEndpointIndex
                      ? "active-row"
                      : undefined
                  }
                >
                  <td>
                    {endpointLabel(endpoint)}
                    {endpoint.index === state.activeEndpointIndex
                      ? " (active)"
                      : ""}
                  </td>
                  <td>
                    <select
                      aria-label={`${endpointLabel(endpoint)} default layer`}
                      value={endpoint.value}
                      disabled={isLoading}
                      onChange={(e) =>
                        setEndpointLayer(
                          endpoint.index,
                          parseInt(e.target.value, 10)
                        )
                      }
                    >
                      {layerValueOptions(state.layerCount, true).map((opt) => (
                        <option key={opt.value} value={opt.value}>
                          {opt.label}
                        </option>
                      ))}
                    </select>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>

          <h3>Per-OS default layer</h3>
          {!state.osDetectionAvailable && (
            <p className="hint">
              OS detection is not enabled in this firmware build. Set a
              connection above to "OS detection" once it is.
            </p>
          )}
          <table className="os-table">
            <thead>
              <tr>
                <th>OS</th>
                <th>Default layer</th>
              </tr>
            </thead>
            <tbody>
              {state.osLayers.map((entry) => (
                <tr key={entry.os}>
                  <td>{OS_LABELS[entry.os] ?? entry.os}</td>
                  <td>
                    <select
                      aria-label={`${OS_LABELS[entry.os] ?? entry.os} default layer`}
                      value={entry.value}
                      disabled={isLoading || !state.osDetectionAvailable}
                      onChange={(e) =>
                        setOsLayer(entry.os, parseInt(e.target.value, 10))
                      }
                    >
                      {layerValueOptions(state.layerCount, false).map((opt) => (
                        <option key={opt.value} value={opt.value}>
                          {opt.label}
                        </option>
                      ))}
                    </select>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>

          <button
            className="btn btn-secondary"
            disabled={isLoading}
            onClick={refresh}
          >
            {isLoading ? "⏳ Refreshing..." : "🔄 Refresh"}
          </button>
        </>
      )}
    </section>
  );
}

export default App;
