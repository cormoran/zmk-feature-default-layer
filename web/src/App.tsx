import { useContext, useMemo, useState } from "react";
import "./App.css";
import { connect as serialConnect } from "@zmkfirmware/zmk-studio-ts-client/transport/serial";
import {
  ZMKConnection,
  ZMKCustomSubsystem,
  ZMKAppContext,
} from "@cormoran/zmk-studio-react-hook";
import {
  DefaultLayerEntry,
  Endpoint,
  Request,
  Response,
  Transport,
} from "./proto/zmk/default_layer/default_layer";

export const SUBSYSTEM_IDENTIFIER = "zmk__default_layer";

const MIN_LAYER = 0;

function App() {
  return (
    <main className="app">
      <header className="app-header">
        <div>
          <p className="eyebrow">ZMK Studio</p>
          <h1>Default Layer</h1>
        </div>
      </header>

      <ZMKConnection
        renderDisconnected={({ connect, isLoading, error }) => (
          <section className="panel">
            <div>
              <h2>Device</h2>
              {error && <p className="error-message">{error}</p>}
            </div>
            <button
              className="button primary"
              disabled={isLoading}
              onClick={() => connect(serialConnect)}
            >
              {isLoading ? "Connecting" : "Connect Serial"}
            </button>
          </section>
        )}
        renderConnected={({ disconnect, deviceName }) => (
          <>
            <section className="panel device-panel">
              <div>
                <p className="eyebrow">Connected</p>
                <h2>{deviceName}</h2>
              </div>
              <button className="button secondary" onClick={disconnect}>
                Disconnect
              </button>
            </section>

            <DefaultLayerPanel />
          </>
        )}
      />
    </main>
  );
}

export function DefaultLayerPanel() {
  const zmkApp = useContext(ZMKAppContext);
  const [entries, setEntries] = useState<DefaultLayerEntry[]>([]);
  const [selectedKey, setSelectedKey] = useState("");
  const [layer, setLayer] = useState(MIN_LAYER);
  const [response, setResponse] = useState("");
  const [isLoading, setIsLoading] = useState(false);

  const subsystem = zmkApp?.findSubsystem(SUBSYSTEM_IDENTIFIER) ?? undefined;
  const selectedEntry = useMemo(
    () => entries.find((entry) => endpointKey(entry.endpoint) === selectedKey),
    [entries, selectedKey]
  );

  const callDefaultLayerRequest = async (
    request: Request
  ): Promise<Response> => {
    const connection = zmkApp?.state.connection;
    if (!connection || !subsystem) {
      throw new Error("Default layer subsystem is not available");
    }

    const service = new ZMKCustomSubsystem(connection, subsystem.index);
    const payload = Request.encode(request).finish();
    const responsePayload = await service.callRPC(payload);
    if (!responsePayload) {
      throw new Error("Empty response");
    }

    return Response.decode(responsePayload);
  };

  const loadDefaultLayers = async () => {
    setIsLoading(true);
    setResponse("");

    try {
      const resp = await callDefaultLayerRequest({
        getDefaultLayers: {},
      });
      if (resp.defaultLayers) {
        setEntries(resp.defaultLayers.entries);
        const active =
          resp.defaultLayers.entries.find((entry) => entry.selected) ??
          resp.defaultLayers.entries[0];
        if (active) {
          setSelectedKey(endpointKey(active.endpoint));
          setLayer(active.layer);
        }
        setResponse("Loaded");
      } else if (resp.error) {
        setResponse(resp.error.message);
      }
    } catch (error) {
      setResponse(error instanceof Error ? error.message : "Request failed");
    } finally {
      setIsLoading(false);
    }
  };

  const saveDefaultLayer = async () => {
    if (!selectedEntry?.endpoint) return;

    setIsLoading(true);
    setResponse("");

    try {
      const resp = await callDefaultLayerRequest({
        setDefaultLayer: {
          endpoint: selectedEntry.endpoint,
          layer,
        },
      });
      if (resp.error) {
        setResponse(resp.error.message);
      } else {
        setResponse(resp.status?.message || "Saved");
        await loadDefaultLayers();
      }
    } catch (error) {
      setResponse(error instanceof Error ? error.message : "Request failed");
    } finally {
      setIsLoading(false);
    }
  };

  if (!zmkApp) return null;

  if (!subsystem) {
    return (
      <section className="panel">
        <h2>Default Layers</h2>
        <p className="error-message">
          Subsystem "{SUBSYSTEM_IDENTIFIER}" not found.
        </p>
      </section>
    );
  }

  return (
    <section className="workspace">
      <div className="toolbar">
        <div>
          <p className="eyebrow">Endpoint Defaults</p>
          <h2>Transport Layer Map</h2>
        </div>
        <button
          className="button secondary"
          disabled={isLoading}
          onClick={loadDefaultLayers}
        >
          Refresh
        </button>
      </div>

      <div className="entry-grid">
        {entries.map((entry) => {
          const key = endpointKey(entry.endpoint);
          return (
            <button
              className={`endpoint-card ${key === selectedKey ? "selected" : ""}`}
              key={key}
              onClick={() => {
                setSelectedKey(key);
                setLayer(entry.layer);
              }}
            >
              <span>{entry.label}</span>
              <strong>Layer {entry.layer}</strong>
              {entry.selected && <small>Active</small>}
            </button>
          );
        })}
      </div>

      <div className="editor">
        <label htmlFor="layer-input">Layer</label>
        <input
          id="layer-input"
          min={MIN_LAYER}
          type="number"
          value={layer}
          onChange={(event) =>
            setLayer(Number.parseInt(event.target.value, 10) || MIN_LAYER)
          }
        />
        <button
          className="button primary"
          disabled={isLoading || !selectedEntry}
          onClick={saveDefaultLayer}
        >
          Save
        </button>
      </div>

      {response && <p className="status-line">{response}</p>}
    </section>
  );
}

function endpointKey(endpoint?: Endpoint): string {
  if (!endpoint) return "none";

  switch (endpoint.transport) {
    case Transport.TRANSPORT_USB:
      return "usb";
    case Transport.TRANSPORT_BLE:
      return `ble-${endpoint.profileIndex}`;
    case Transport.TRANSPORT_NONE:
    default:
      return "none";
  }
}

export default App;
