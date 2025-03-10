import { serve } from '@hono/node-server'
import { Hono } from 'hono'

const app = new Hono()

let cars = {} as Record<string, Boolean>

app.get('/car/:id', (c) => {
  return c.text((cars[c.req.param("id")] || false) ? "true" : "false")
});

app.put('/car/:id', (c) => {
  cars[c.req.param("id")] = true;
  return c.text("OK");
});

app.delete('/car/:id', (c) => {
  cars[c.req.param("id")] = false;
  return c.text("OK");
});

setInterval(() => {
  console.log(cars)
}, 1000)

serve({
  fetch: app.fetch,
  port: 7452
}, (info) => {
  console.log(`Server is running on http://localhost:${info.port}`)
})
