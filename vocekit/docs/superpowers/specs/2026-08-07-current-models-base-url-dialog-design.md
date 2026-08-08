# Current model catalog, provider Base URLs, and custom-model dialog design

## Goal

Keep Vocekit's selectable OpenAI and Anthropic models aligned with the current official API catalogs, preserve existing user functions through explicit legacy-ID migration, allow official providers to use user-supplied API gateways, and eliminate Chinese text clipping in the custom-model dialog.

## Model catalog

The built-in catalog will expose these OpenAI models:

- `openai:gpt-5.6-sol` — GPT-5.6 Sol
- `openai:gpt-5.6-terra` — GPT-5.6 Terra
- `openai:gpt-5.6-luna` — GPT-5.6 Luna

The built-in catalog will expose these Anthropic models:

- `claude:claude-fable-5` — Claude Fable 5
- `claude:claude-opus-5` — Claude Opus 5
- `claude:claude-sonnet-5` — Claude Sonnet 5
- `claude:claude-haiku-4-5` — Claude Haiku 4.5

GPT-5.5, GPT-5.4, GPT-5.4 Mini, Claude Opus 4.8, Claude Opus 4.7, and Claude Sonnet 4.6 will not appear in model selectors. DeepSeek and user-defined model entries remain unchanged.

Official references:

- OpenAI model catalog: <https://developers.openai.com/api/docs/models>
- GPT-5.6 Chat Completions support: <https://developers.openai.com/api/docs/models/gpt-5.6-sol>
- Anthropic model overview: <https://platform.claude.com/docs/en/about-claude/models/overview>

## Legacy model migration

Legacy IDs remain accepted as input, but normalize to a visible current model before a selector chooses an item or a provider sends a request:

| Legacy model ID | Current model ID |
| --- | --- |
| `openai:gpt-5.5` | `openai:gpt-5.6-sol` |
| `openai:gpt-5.4` | `openai:gpt-5.6-terra` |
| `openai:gpt-5.4-mini` | `openai:gpt-5.6-luna` |
| Any `openai:gpt-4*` or unprefixed `gpt-4*` ID | `openai:gpt-5.6-terra` |
| `claude:claude-opus-4-8` | `claude:claude-opus-5` |
| `claude:claude-opus-4-7` | `claude:claude-opus-5` |
| `claude:claude-sonnet-4-6` | `claude:claude-sonnet-5` |
| Any retired Claude 3.x ID | `claude:claude-sonnet-5` |

Unknown OpenAI-prefixed or unprefixed `gpt-*` values fall back to GPT-5.6 Terra. Unknown Claude-prefixed or unprefixed `claude-*` values fall back to Claude Sonnet 5. Explicit current model IDs and custom provider IDs are preserved. Existing settings are rewritten only through the application's normal save path; merely loading settings does not perform an unrelated file write.

## Official-provider Base URLs

`SecretConfig` and the secret store will add two optional values:

- `openai_base_url`
- `anthropic_base_url`

The interface settings page places a plain-text Base URL field directly below each provider's API-key row.

When the value is empty, the provider uses its official endpoint:

- OpenAI: `https://api.openai.com/v1/chat/completions`
- Anthropic: `https://api.anthropic.com/v1/messages`

OpenAI accepts a host root, a `/v1` base, or a complete `/v1/chat/completions` endpoint. Anthropic accepts a host root, a `/v1` base, or a complete `/v1/messages` endpoint. Normalization is deterministic and never appends the endpoint twice. Invalid or hostless URLs produce a configuration error before a network request starts.

The provider's self-check and normal completion path use the same resolved URL. API keys remain handled by the existing secret store and are never displayed in a normalized-URL preview.

## Custom-model dialog

The existing profile URL remains the source of truth. Its visible label becomes `API URL（接口地址）`, and its example uses a complete OpenAI-compatible endpoint such as `https://api.example.com/v1/chat/completions`. Supporting text states that a root address is also accepted and automatically completed.

The dialog displays a non-secret `最终请求地址` preview derived from the current URL input. Empty or invalid input shows a concise status instead of a misleading endpoint. The preview updates while the user edits the URL.

## Chinese button sizing

The custom-model dialog will not apply the padded global button style to a 32-pixel fixed-height button. `新增模型`, `测试`, `删除`, `取消`, and `保存` use a dialog-specific compact action style with no vertical padding and a minimum height computed from the current font metrics, never below 40 pixels. Width remains content-driven with sufficient horizontal padding.

This is intentionally scoped to this dialog so unrelated pages do not change appearance.

## Error handling and compatibility

- A missing Base URL is valid and selects the official endpoint.
- An invalid non-empty Base URL blocks the provider self-check and request with a provider-configuration error.
- A custom model without a URL remains excluded from model selectors, matching current behavior.
- Legacy IDs are migrated by normalization rather than kept as hidden selectable items.
- Provider protocol and response parsing remain unchanged: OpenAI continues to use Chat Completions, Anthropic continues to use Messages, and custom profiles continue to use the OpenAI-compatible Chat Completions shape.

## Verification

Automated tests will cover:

- exact current built-in model entries and absence of retired entries;
- each legacy-to-current normalization mapping;
- provider default models;
- OpenAI and Anthropic root, `/v1`, full-endpoint, empty, and invalid URL handling;
- secret-store round trips for both Base URLs;
- custom-model final-URL preview normalization;
- minimum button height relative to font metrics.

The release build must pass the full existing test suite. Visual verification must inspect the custom-model dialog with one and multiple profiles at normal scale and increased Windows text scaling, confirming that `测试` and `删除` are fully visible and that long URLs remain usable without horizontal layout breakage.

## Out of scope

- Removing OpenAI or Anthropic provider support.
- Migrating the OpenAI provider from Chat Completions to Responses API.
- Discovering models dynamically from a provider account.
- Changing DeepSeek model names or custom-model request/response schemas.
