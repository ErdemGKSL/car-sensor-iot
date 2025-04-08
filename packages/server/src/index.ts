import { createNodeWebSocket } from '@hono/node-ws'
import { Hono } from 'hono'
import { serve } from '@hono/node-server'
import { randomBytes } from 'crypto'

// Global state for storing device and sensor data
interface SensorData {
  value: number;
  lastUpdated: number;
}

interface Device {
  connected: boolean;
  lastSeen: number;
  sensors: Record<string, SensorData>;
  token: string; // New token field for authentication
}

// Global stores - no database needed
const devices: Record<string, Device> = {};
// Lookup map for device tokens
const tokenToDeviceId: Record<string, string> = {};

// Global store for frontend WebSocket clients
let frontendClients: any[] = [];

// Create Hono app
const app = new Hono()

// Setup WebSocket with Hono
const { injectWebSocket, upgradeWebSocket } = createNodeWebSocket({ app })

// HTTP endpoints
app.get('/', (c) => c.json({ message: 'Car Sensor IoT Server' }))
app.get('/devices', (c) => c.json(devices))
app.get('/device/:id', (c) => {
  const deviceId = c.req.param('id')
  return devices[deviceId] 
    ? c.json(devices[deviceId])
    : c.json({ error: 'Device not found' }, 404)
})
app.get('/device/:deviceId/sensor/:sensorId', (c) => {
  const deviceId = c.req.param('deviceId')
  const sensorId = c.req.param('sensorId')
  
  if (devices[deviceId]?.sensors[sensorId]) {
    return c.json(devices[deviceId].sensors[sensorId])
  }
  return c.json({ error: 'Device or sensor not found' }, 404)
})

// HTTP endpoints for Arduino devices
app.post('/api/register', async (c) => {
  try {
    const data = await c.req.json()
    const { deviceId, token } = handleDeviceRegistration(data)
    return c.json({ success: true, deviceId, token })
  } catch (error) {
    console.error('Error processing registration:', error)
    return c.json({ error: 'Invalid request' }, 400)
  }
})

app.post('/api/data', async (c) => {
  try {
    const data = await c.req.json()
    const { token, sensor_id, value } = data
    
    if (!token) {
      return c.json({ error: 'Missing token' }, 400)
    }
    
    const deviceId = getDeviceIdByToken(token)
    if (!deviceId) {
      return c.json({ error: 'Invalid token' }, 401)
    }
    
    // Update device last seen timestamp
    if (devices[deviceId]) {
      devices[deviceId].lastSeen = Date.now()
      devices[deviceId].connected = true
      
      // Handle sensor data if provided
      if (sensor_id !== undefined && value !== undefined) {
        handleSensorData(deviceId, { sensor_id, value })
      }
      
      return c.json({ success: true })
    } else {
      return c.json({ error: 'Device not registered' }, 404)
    }
  } catch (error) {
    console.error('Error processing data:', error)
    return c.json({ error: 'Invalid request' }, 400)
  }
})

app.post('/api/heartbeat', async (c) => {
  try {
    const data = await c.req.json()
    const { token } = data
    
    if (!token) {
      return c.json({ error: 'Missing token' }, 400)
    }
    
    const deviceId = getDeviceIdByToken(token)
    if (!deviceId || !devices[deviceId]) {
      return c.json({ error: 'Invalid token' }, 401)
    }
    
    // Update last seen time
    devices[deviceId].lastSeen = Date.now()
    devices[deviceId].connected = true
    
    return c.json({ success: true })
  } catch (error) {
    console.error('Error processing heartbeat:', error)
    return c.json({ error: 'Invalid request' }, 400)
  }
})

// Frontend WebSocket endpoint
app.get('/ws', upgradeWebSocket((c) => {
  return {
    onOpen(_, ws) {
      frontendClients.push(ws);
      console.log('Frontend client connected');
    },
    onMessage(event, ws) {
      // Check if the message is binary (heartbeat)
      if (event.data instanceof Buffer || event.data instanceof ArrayBuffer) {
        // Respond to heartbeat with binary response
        const response = new Uint8Array(1);
        response[0] = 1;
        try {
          ws.send(response);
        } catch (err) {
          console.error('Error sending heartbeat response:', err);
        }
        return;
      }
      
      // Handle JSON messages
      console.log(`Message from frontend: ${event.data.toString()}`);
    },
    onClose(_, ws) {
      frontendClients = frontendClients.filter(client => client !== ws);
      console.log('Frontend client disconnected');
    }
  }
}));

// Helper function to find device ID by token
function getDeviceIdByToken(token: string): string | null {
  return tokenToDeviceId[token] || null;
}

// Generate a secure random token
function generateToken(): string {
  return randomBytes(32).toString('hex');
}

// Handle device registration
function handleDeviceRegistration(data: any): { deviceId: string, token: string } {
  const { device_id, sensors } = data
  
  // Generate a unique token for this device
  const token = generateToken();
  
  // Initialize or update device record
  if (!devices[device_id]) {
    devices[device_id] = {
      connected: true,
      lastSeen: Date.now(),
      sensors: {},
      token: token
    }
  } else {
    devices[device_id].connected = true
    devices[device_id].lastSeen = Date.now()
    devices[device_id].token = token // Update token if device re-registers
    
    // Remove old token mapping if exists
    Object.keys(tokenToDeviceId).forEach(oldToken => {
      if (tokenToDeviceId[oldToken] === device_id) {
        delete tokenToDeviceId[oldToken];
      }
    });
  }
  
  // Store the token-to-deviceId mapping
  tokenToDeviceId[token] = device_id;
  
  // Register sensors
  if (Array.isArray(sensors)) {
    sensors.forEach((sensor: any) => {
      const sensorId = sensor.sensor_id
      if (!devices[device_id].sensors[sensorId]) {
        devices[device_id].sensors[sensorId] = {
          value: 0,
          lastUpdated: 0
        }
      }
    })
  }
  
  console.log(`Device ${device_id} registered with token ${token.substring(0, 8)}... and sensors:`, 
    Object.keys(devices[device_id].sensors).join(', '))
  
  // Broadcast connection event to frontend clients
  broadcastUpdate({
    type: 'deviceStatus',
    deviceId: device_id,
    connected: true,
    timestamp: Date.now()
  });

  if (Array.isArray(sensors)) {
    sensors.forEach((sensor: any) => {
      const sensorId = sensor.sensor_id
      broadcastUpdate({
        type: 'sensorData',
        deviceId: device_id,
        sensor_id: sensorId,
        value: 0,
        lastUpdated: 0
      });
    })
  }
  
  return { deviceId: device_id, token };
}

// Handle sensor data updates
function handleSensorData(deviceId: string, data: any) {
  const { sensor_id, value } = data
  
  if (devices[deviceId]?.sensors[sensor_id]) {
    // Update sensor data
    devices[deviceId].sensors[sensor_id].value = value
    devices[deviceId].sensors[sensor_id].lastUpdated = Date.now()
    devices[deviceId].lastSeen = Date.now()
    console.log(`Updated sensor ${sensor_id} of device ${deviceId} with value ${value}`)
    
    // Broadcast the update to frontend clients
    broadcastUpdate({ 
      type: 'sensorData',
      deviceId, 
      sensor_id, 
      value, 
      lastUpdated: devices[deviceId].sensors[sensor_id].lastUpdated 
    });
  } else {
    console.warn(`Received data for unknown device/sensor: ${deviceId}/${sensor_id}`)
  }
}

// Function to broadcast updates to all connected frontend clients
function broadcastUpdate(update: any) {
  const message = JSON.stringify(update);
  frontendClients.forEach(ws => {
    try {
      ws.send(message);
    } catch (err) {
      console.error('Error sending message to frontend client:', err);
    }
  });
}

// Function to check for inactive devices
function checkInactiveDevices() {
  const now = Date.now();
  const TIMEOUT = 30 * 1000; // 30 seconds in milliseconds
  
  Object.entries(devices).forEach(([deviceId, device]) => {
    if (device.connected && now - device.lastSeen > TIMEOUT) {
      // Mark device as disconnected
      device.connected = false;
      console.log(`Device ${deviceId} marked as disconnected due to inactivity`);
      
      // Notify frontend clients
      broadcastUpdate({
        type: 'deviceStatus',
        deviceId,
        connected: false,
        timestamp: now
      });
    }
  });
}

// Start the server
const PORT = 17452
const server = serve({ port: PORT, fetch: app.fetch }, (info) => {
  console.log(`HTTP & WebSocket Server running on http://localhost:${info.port}`)
  console.log(`WebSocket server available at ws://localhost:${info.port}/ws`)
  
  // Set up periodic check for inactive devices
  setInterval(checkInactiveDevices, 5000); // Check every 5 seconds
})

// Inject WebSocket handler into the server
injectWebSocket(server)