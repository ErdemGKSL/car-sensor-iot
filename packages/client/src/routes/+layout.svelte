<script lang="ts">
    import { browser } from '$app/environment';
	import { setupWebsocket, data as states } from '$lib/index.svelte';
    import { onMount } from 'svelte';
	import '../app.css';
	let { children, data } = $props();

	onMount(() => {
		if (browser) {
			if (data.devices) {
				states.devices = data.devices;
			}
			
			setupWebsocket()
		}
	})
</script>

{@render children()}
