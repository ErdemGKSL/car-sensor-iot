<script lang="ts">
  import { data, setupWebsocket } from "$lib/index.svelte";
  import { onMount } from "svelte";

  // Format timestamp to readable date/time
  function formatTimestamp(timestamp: number): string {
    return new Date(timestamp).toLocaleString();
  }

  let calculateFrom = $state(Date.now());

  setInterval(() => {
    calculateFrom = Date.now();
  }, 1000);


  // Calculate time difference from now in a human-readable format
  function getTimeAgo(timestamp: number): string {
    const seconds = Math.floor((calculateFrom - timestamp) / 1000);
    
    if (seconds < 60) return `${seconds} seconds ago`;
    
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `${minutes} minute${minutes !== 1 ? 's' : ''} ago`;
    
    const hours = Math.floor(minutes / 60);
    if (hours < 24) return `${hours} hour${hours !== 1 ? 's' : ''} ago`;
    
    const days = Math.floor(hours / 24);
    return `${days} day${days !== 1 ? 's' : ''} ago`;
  }


  // Initialize the WebSocket connection when the page loads
  onMount(() => {
    setupWebsocket();
    return () => {
      // Cleanup when component is destroyed
      if (data.heartbeatInterval) {
        clearInterval(data.heartbeatInterval);
      }
      if (data.websocket) {
        data.websocket.close();
      }
    };
  });

  // Derived value to get devices as an array with their IDs
  let devices = $derived(
    Object.entries(data.devices)
      .map(([id, device]) => ({ id, ...device }))
      .sort((a, b) => b.lastSeen - a.lastSeen) // Sort by most recently seen
  );
</script>

<header>
  <h1>Car Sensor Dashboard</h1>
  <div class="connection-status {data.connectionStatus}">
    {data.connectionStatus === 'connected' ? '🟢 Connected to server' : 
     data.connectionStatus === 'connecting' ? '🟠 Connecting...' : '🔴 Disconnected'}
  </div>
</header>

<main>
  {#if devices.length === 0}
    <div class="no-devices">
      <h2>No devices detected</h2>
      <p>Connect an IoT device to see its data here</p>
    </div>
  {:else}
    <div class="device-grid">
      {#each devices as device (device.id)}
        <div class="device-card">
          <div class="device-header">
            <h2>{device.id}</h2>
            <span class="device-status {device.connected ? 'connected' : 'disconnected'}">
              {device.connected ? '●' : '○'} {device.connected ? 'Online' : 'Offline'}
            </span>
          </div>
          
          <div class="sensors">
            {#each Object.entries(device.sensors) as [sensorId, sensor]}
              <div class="sensor-card">
                <div class="sensor-header">
                  <h3>{sensorId}</h3>
                </div>
                <div class="sensor-value">
                  <span class="value">{sensor.value}</span>
                </div>
                <div class="sensor-meta">
                  <span class="timestamp" title={formatTimestamp(sensor.lastUpdated)}>
                    Updated: {getTimeAgo(sensor.lastUpdated)}
                  </span>
                </div>
              </div>
            {/each}
          </div>
          
          <!-- Device footer removed as requested -->
        </div>
      {/each}
    </div>
  {/if}
</main>

<style lang="scss">
  // Bootstrap-inspired colors
  $primary: #0d6efd;
  $secondary: #6c757d;
  $success: #198754;
  $danger: #dc3545;
  $warning: #ffc107;
  $info: #0dcaf0;
  $light: #f8f9fa;
  $dark: #212529;
  
  // Card and spacing variables
  $border-radius: 8px;
  $shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  $spacing: 1rem;
  
  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: $spacing;
    border-bottom: 1px solid lighten($secondary, 30%);
    margin-bottom: $spacing * 2;
    
    h1 {
      margin: 0;
      color: $primary;
      font-weight: 600;
    }
    
    .connection-status {
      padding: $spacing * 0.5 $spacing;
      border-radius: $border-radius;
      font-weight: 500;
      
      &.connected {
        color: $success;
      }
      
      &.connecting {
        color: $warning;
      }
      
      &.disconnected {
        color: $danger;
      }
    }
  }
  
  main {
    padding: 0 $spacing;
  }
  
  .no-devices {
    text-align: center;
    padding: $spacing * 3;
    background-color: $light;
    border-radius: $border-radius;
    box-shadow: $shadow;
    
    h2 {
      color: $secondary;
      margin-bottom: $spacing;
    }
    
    p {
      color: lighten($secondary, 10%);
    }
  }
  
  .device-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
    grid-gap: $spacing * 2;
  }
  
  .device-card {
    background-color: white;
    border-radius: $border-radius;
    box-shadow: $shadow;
    overflow: hidden;
    transition: transform 0.2s ease;
    
    &:hover {
      transform: translateY(-5px);
    }
    
    .device-header {
      padding: $spacing;
      background-color: $light;
      border-bottom: 1px solid lighten($secondary, 35%);
      display: flex;
      justify-content: space-between;
      align-items: center;
      
      h2 {
        margin: 0;
        font-size: 1.25rem;
        color: $dark;
        font-weight: 600;
      }
      
      .device-status {
        font-weight: 500;
        display: flex;
        align-items: center;
        gap: 0.3rem;
        
        &.connected {
          color: $success;
        }
        
        &.disconnected {
          color: $danger;
        }
      }
    }
    
    .sensors {
      padding: $spacing;
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
      grid-gap: $spacing;
    }
    
    .sensor-card {
      padding: $spacing;
      background-color: $light;
      border-radius: $border-radius;
      text-align: center;
      
      .sensor-header h3 {
        margin: 0 0 $spacing * 0.5 0;
        font-size: 1rem;
        color: $secondary;
        font-weight: 500;
      }
      
      .sensor-value {
        padding: $spacing * 0.5;
        
        .value {
          font-size: 1.5rem;
          font-weight: 700;
          color: $primary;
        }
      }
      
      .sensor-meta {
        font-size: 0.8rem;
        color: lighten($secondary, 10%);
        white-space: nowrap; // Prevent text wrapping
        overflow: hidden;
        text-overflow: ellipsis;
      }
    }
    
    // Device footer CSS removed as requested
  }

  // Responsive tweaks
  @media (max-width: 768px) {
    header {
      flex-direction: column;
      align-items: flex-start;
      gap: $spacing;
    }
    
    .device-grid {
      grid-template-columns: 1fr;
    }
  }
</style>