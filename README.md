# Car Sensor IoT

A monorepo for a car sensor IoT project containing:

- Server package: Backend services
- Client package: Frontend application
- Arduino package: Code for Arduino devices

## Setup

This project uses pnpm workspaces to manage the monorepo and TypeScript for development.

```bash
# Install pnpm if you don't have it already
npm install -g pnpm

# Install all dependencies
pnpm install

# Run development servers
pnpm dev:server
pnpm dev:client
```

## Project Structure

```
car-sensor-iot/
├── packages/
│   ├── server/       # Backend server code
│   │   └── src/      # TypeScript source files
│   ├── client/       # Frontend client code
│   │   └── src/      # TypeScript source files
│   └── arduino/      # Arduino device code
│       └── src/      # TypeScript source files
```

## Development

All packages use TypeScript and can be run with the `tsx` command:

```bash
# Running individual packages during development
cd packages/server && pnpm dev
cd packages/client && pnpm dev

# Building TypeScript code
pnpm build
```
