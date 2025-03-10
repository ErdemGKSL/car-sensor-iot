import { BASE_API_URL, type Device } from "$lib/index.svelte"
import type { ServerLoad } from "@sveltejs/kit";

export const load: ServerLoad = async ({ fetch,  }) => {
  return {
    devices: await fetch(`https://${BASE_API_URL}/devices`).then(r => r.json()).catch(() => []) as Record<string, Device>
  }
};