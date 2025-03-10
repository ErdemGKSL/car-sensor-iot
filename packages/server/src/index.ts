import { createNodeWebSocket } from '@hono/node-ws'
import { Hono } from 'hono'
import { serve } from '@hono/node-server'

// Global state for storing device and sensor data
interface SensorData {
  value: number;
  lastUpdated: number;
}

interface Device {
  connected: boolean;
  lastSeen: number;
  sensors: Record<string, SensorData>;
}

// Global stores - no database needed
const devices: Record<string, Device> = {};

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

// WebSocket endpoint for arduino
app.get('/arduino-ws', upgradeWebSocket((c) => {
  // Store the device ID for this connection
  let deviceId: string | null = null
  
  return {
    // Called when a WebSocket message is received
    onMessage(event, ws) {
      try {
        const data = JSON.parse(event.data.toString())
        console.log('Received message:', data)
        
        switch (data.type) {
          case 'register':
            // Handle device registration
            deviceId = handleDeviceRegistration(data)
            break
          case 'data':
            // Handle sensor data update
            if (deviceId) {
              handleSensorData(deviceId, data)
            } else {
              console.warn('Received data before device registration')
            }
            break
          default:
            console.warn('Unknown message type:', data.type)
        }
      } catch (error) {
        console.error('Error processing message:', error)
      }
    },
    
    // Called when WebSocket connection is opened
    onOpen(ws) {
      console.log('WebSocket client connected')
    },
    
    // Called when WebSocket connection is closed
    onClose() {
      if (deviceId) {
        console.log(`Device ${deviceId} disconnected`)
        if (devices[deviceId]) {
          devices[deviceId].connected = false
        }
        deviceId = null
      }
    }
  }
}))

// Frontend WebSocket endpoint
app.get('/ws', upgradeWebSocket((c) => {
  return {
    onOpen(ws) {
      frontendClients.push(ws);
      console.log('Frontend client connected');
    },
    onMessage(event, ws) {
      console.log(`Message from frontend: ${event.data.toString()}`);
    },
    onClose(ws) {
      frontendClients = frontendClients.filter(client => client !== ws);
      console.log('Frontend client disconnected');
    }
  }
}));

// Handle device registration
function handleDeviceRegistration(data: any): string {
  const { device_id, sensors } = data
  
  // Initialize or update device record
  if (!devices[device_id]) {
    devices[device_id] = {
      connected: true,
      lastSeen: Date.now(),
      sensors: {}
    }
  } else {
    devices[device_id].connected = true
    devices[device_id].lastSeen = Date.now()
  }
  
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
  
  console.log(`Device ${device_id} registered with sensors:`, 
    Object.keys(devices[device_id].sensors).join(', '))
  
  return device_id
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
    broadcastUpdate({ deviceId, sensor_id, value, lastUpdated: devices[deviceId].sensors[sensor_id].lastUpdated });
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

// Start the server
const PORT = 7452
const server = serve({ port: PORT, fetch: app.fetch }, (info) => {
  console.log(`HTTP & WebSocket Server running on http://localhost:${info.port}`)
  console.log(`WebSocket server available at ws://localhost:${info.port}/ws`)
})

// Inject WebSocket handler into the server
injectWebSocket(server)

// Log current state periodically
setInterval(() => {
  console.log('Current devices:', JSON.stringify(devices, null, 2))
}, 5000)
