# Instructions for AI Coding Agents

Mid Engine is an early-stage, local-first game engine and editor. Treat this file as project-level guidance for coding agents.

## Before changing code
- Read `README.md`, `docs/AGENTS.md`, and the relevant tests.
- Preserve the no-runtime-dependency approach unless a dependency clearly earns its cost.
- Prefer small, testable modules and explicit JSON contracts.
- Do not claim a renderer, physics engine, native desktop shell, MCP server, or LLM-powered autonomous agent exists unless it has actually been implemented and tested.

## Architecture boundaries
- `engine/`: provider-independent scene/entity data model and validation.
- `server/`: local HTTP API, tool registry, command adapters, persistence.
- `app.js`, `index.html`, `styles.css`: browser editor.
- `tests/`: built-in Node test runner.
- `docs/AGENTS.md`: integration contract for external agents.

## Safety rules
- Never add arbitrary shell execution or unrestricted filesystem access to agent tools.
- Keep project writes inside the project root and validate all external input.
- Any destructive tool must require explicit user approval; never infer approval from an agent's own text.
- Do not expose the local API to LAN or public networks without authentication, authorization, and a security review.
- Return structured errors and verify state after operations.
- Do not store secrets in source control or project scene data.

## Required checks
Run `npm test` and `npm run check` after changes. Add regression tests for every bug fix and for tool validation / permission changes.
