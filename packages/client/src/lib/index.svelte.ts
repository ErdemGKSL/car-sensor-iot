// Types for sensor data and devices
export interface SensorData {
  value: number;
  lastUpdated: number;
}

export interface Device {
  connected: boolean;
  lastSeen: number;
  sensors: Record<string, SensorData>;
}

// Export constants and state
export const BASE_API_URL = "iot.erdemdev.tr";

// Using Svelte 5's $state for reactivity with a single state object
export let data = $state({
  websocket: null as WebSocket | null,
  devices: {} as Record<string, Device>,
  connectionStatus: 'disconnected' as 'connected' | 'disconnected' | 'connecting',
  heartbeatInterval: null as number | null
});

// WebSocket setup function
export const setupWebsocket = () => {
  // Close any existing connection
  if (data.websocket && data.websocket.readyState !== WebSocket.CLOSED) {
    data.websocket.close();
  }
  
  // Clear any existing heartbeat interval
  if (data.heartbeatInterval) {
    clearInterval(data.heartbeatInterval);
    data.heartbeatInterval = null;
  }

  data.connectionStatus = 'connecting';
  
  // Create secure or non-secure connection based on current protocol
  const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
  const url = `${protocol}://${BASE_API_URL}/ws`;
  
  const ws = new WebSocket(url);
  
  ws.onopen = () => {
    console.log('WebSocket connected');
    data.connectionStatus = 'connected';
    
    // Request initial data
    ws.send(JSON.stringify({ type: 'getDevices' }));
    
    // Set up heartbeat to keep connection alive
    data.heartbeatInterval = window.setInterval(() => {
      if (ws.readyState === WebSocket.OPEN) {
        // Send a binary heartbeat message
        const heartbeat = new Uint8Array(1);
        heartbeat[0] = 1; // Value doesn't matter, just sending a simple binary message
        ws.send(heartbeat);
        console.debug('Heartbeat sent');
      }
    }, 15000) as unknown as number; // Send heartbeat every 15 seconds
  };
  
  ws.onmessage = (event) => {
    try {
      // Skip processing binary messages (heartbeat responses)
      if (event.data instanceof Blob || event.data instanceof ArrayBuffer) {
        console.debug('Received binary message (likely heartbeat response)');
        return;
      }
      
      const payload = JSON.parse(event.data);
      
      // Handle different types of messages from server
      if (payload.type === 'deviceStatus') {
        // Handle device connection/disconnection events
        updateDeviceStatus(payload);
      } else if (payload.type === 'sensorData') {
        // Single sensor update
        updateSensorData(payload);
      } else if (payload.type === 'fullUpdate') {
        // Full devices update
        data.devices = payload.devices;
      } else if (payload.deviceId && payload.sensor_id) {
        // For backwards compatibility with old message format
        updateSensorData(payload);
      }
    } catch (error) {
      console.error('Error processing WebSocket message:', error);
    }
  };
  
  ws.onclose = () => {
    console.log('WebSocket disconnected');
    data.connectionStatus = 'disconnected';
    
    // Clear heartbeat interval
    if (data.heartbeatInterval) {
      clearInterval(data.heartbeatInterval);
      data.heartbeatInterval = null;
    }
    
    // Attempt to reconnect after delay
    setTimeout(setupWebsocket, 5000);
  };
  
  ws.onerror = (error) => {
    console.error('WebSocket error:', error);
  };
  
  // Store the websocket instance
  data.websocket = ws;
};

// Helper function to update device connection status
function updateDeviceStatus(payload: { deviceId: string, connected: boolean, timestamp: number }) {
  const { deviceId, connected, timestamp } = payload;
  
  // Create a new devices object to ensure reactivity in Svelte 5
  const updatedDevices = { ...data.devices };
  
  // If the device doesn't exist yet, create it
  if (!updatedDevices[deviceId]) {
    updatedDevices[deviceId] = {
      connected: connected,
      lastSeen: timestamp,
      sensors: {}
    };
  } else {
    // Update the device connection status
    updatedDevices[deviceId].connected = connected;
    updatedDevices[deviceId].lastSeen = timestamp;
  }
  
  // Update the state to trigger reactivity
  data.devices = updatedDevices;
  
  console.log(`Device ${deviceId} is now ${connected ? 'connected' : 'disconnected'}`);
}

// Helper function to update sensor data
function updateSensorData(payload: { deviceId: string, sensor_id: string, value: number, lastUpdated: number }) {
  const { deviceId, sensor_id, value, lastUpdated } = payload;
  
  // Create a new devices object to ensure reactivity in Svelte 5
  const updatedDevices = { ...data.devices };
  
  // If the device doesn't exist yet, create it
  if (!updatedDevices[deviceId]) {
    updatedDevices[deviceId] = {
      connected: true,
      lastSeen: lastUpdated,
      sensors: {}
    };
  }
  
  // If the sensor doesn't exist yet, create it
  if (!updatedDevices[deviceId].sensors[sensor_id]) {
    updatedDevices[deviceId].sensors[sensor_id] = { value: 0, lastUpdated: 0 };
  }
  
  // Update the sensor data
  updatedDevices[deviceId].sensors[sensor_id].value = value;
  updatedDevices[deviceId].sensors[sensor_id].lastUpdated = lastUpdated;
  updatedDevices[deviceId].lastSeen = lastUpdated;
  
  // Update the state to trigger reactivity
  data.devices = updatedDevices;
}

// Function to manually fetch devices if needed
export async function fetchDevices(): Promise<Record<string, Device> | undefined> {
  try {
    const protocol = window.location.protocol;
    const url = `https://${BASE_API_URL}/devices`;
    const response = await fetch(url);
    if (!response.ok) throw new Error('Failed to fetch devices');
    const fetchedData = await response.json();
    return fetchedData;
  } catch (error) {
    console.error('Error fetching devices:', error);
  }
}
