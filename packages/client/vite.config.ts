import tailwindcss from "@tailwindcss/vite";
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig, preview } from 'vite';

export default defineConfig({
	plugins: [sveltekit(), tailwindcss()],
	preview: {
		allowedHosts: ['iot.erdemdev.tr']
	}
});
