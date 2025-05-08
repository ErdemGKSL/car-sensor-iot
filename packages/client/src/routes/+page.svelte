<script lang="ts">
    import { page } from "$app/state";
  import { map, data, type MapFillable } from "$lib/index.svelte";

  // Helper to get color for a spot based on device/sensor state
  function getSpotColor(spot: MapFillable) {
    if ("color" in spot && "deviceId" in spot && "sensorId" in spot) {
      const device = data.devices[spot.deviceId];
      if (!device) return spot.color.notAvailable;
      const sensor = device.sensors[spot.sensorId];
      if (!sensor || sensor.lastUpdated === 0) return spot.color.notAvailable;
      if (Date.now() - sensor.lastUpdated > 30000) return spot.color.notAvailable;
      return (sensor.value > 60) ? spot.color.on : spot.color.off;
    }
    // For road or static elements
    return spot.color;
  }

  let userPosition = $derived({
    x: page.url.searchParams.get("x") || -1,
    y: page.url.searchParams.get("y") || -1
  });
  // you can acces userPosition.x and userPosition.y
  // to get the coordinates of the user
</script>

<div class="flex justify-center items-center min-h-screen bg-gray-100">
  <div class="w-[100vw] max-w-[800px] aspect-[3/2] bg-white rounded-xl shadow-lg flex items-center justify-center relative overflow-hidden">
    <svg
      viewBox="0 0 150 150"
      class="w-full h-full"
      style="display: block;"
    >
      {#each map as spot, i (spot.id)}
        <rect
          x={spot.x}
          y={spot.y}
          width={spot.width}
          height={spot.height}
          rx="6"
          fill={getSpotColor(spot)}
          stroke="#222"
          stroke-width="1"
        >
            {#if "deviceId" in spot && "sensorId" in spot}
                <title>
                    {spot.sensorId}
                </title>
            {/if}
        </rect>
        <!-- {#if "deviceId" in spot && "sensorId" in spot}
          <text
            x={spot.x + spot.width / 2}
            y={spot.y + spot.height / 2 + 4}
            text-anchor="middle"
            font-size="8"
            fill="#222"
            font-family="sans-serif"
            pointer-events="none"
          >
            {spot.sensorId}
          </text>
        {/if} -->
      {/each}
      {#if +userPosition.x !== -1 && +userPosition.y !== -1}
        <circle
          cx={+userPosition.x}
          cy={+userPosition.y}
          r="4"
          fill="#2563eb"
          stroke="#fff"
          stroke-width="1.5"
        >
          <title>User Position</title>
        </circle>
      {/if}
    </svg>
  </div>
</div>
