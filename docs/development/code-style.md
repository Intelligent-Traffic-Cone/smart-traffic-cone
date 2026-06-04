# Code Style

## C and C++

- Use C++17-compatible code for PlatformIO firmware and cone device components.
- Public hardware headers live under `components/cone_device/include`.
- Private implementation stays under `components/cone_device/src`.
- Public status structs return snapshots, not references to mutable state.
- Error strings use short snake_case values such as `disabled` or
  `uart_unavailable`.

## Python

- Use type hints for FastAPI route inputs and outputs.
- Keep Pydantic models in `models.py`.
- Prefer explicit enums for public status and risk values.

## Frontend

- Keep `apps/dispatch-web` static until a frontend framework is deliberately
  selected.
- Do not commit real map keys or security codes.
- If a real map key is needed, use `config.local.js`.

## Markdown

- Use UTF-8.
- Keep developer docs under `docs/development`.
- Keep product/reference materials under `docs/product`.

## Commits

Use short imperative commit messages, for example:

```text
Set up edge cone firmware skeleton
```
