"""Anthropic-endpoint compatibility shim for openevolve's stock OpenAI LLM
client.

openevolve's OpenAILLM (llm/openai.py) unconditionally puts a "temperature"
key (and, if top_p is set, a "top_p" key) in every request body -- there's
no config knob to omit it. That's fine against OpenAI itself, but Claude
4.7+ (including Sonnet 5/Haiku 4.5) reject the request with a 400 the
moment either key is *present*, regardless of its value: "temperature is
deprecated for this model". Anthropic's own guidance is to drop the
parameter entirely and steer behavior via prompting instead.

OpenAILLM's request-building and its actual API call are two separate
methods (generate_with_context() builds `params`, _call_api() sends them),
so the params dict already exists by the time _call_api() runs -- the
smallest possible fix is overriding just that one method to strip the
offending keys first.
"""

from typing import Any, Dict

from openevolve.llm.openai import OpenAILLM

_UNSUPPORTED_PARAMS = ("temperature", "top_p")


class AnthropicCompatLLM(OpenAILLM):
    async def _call_api(self, params: Dict[str, Any]) -> str:
        params = {k: v for k, v in params.items() if k not in _UNSUPPORTED_PARAMS}
        return await super()._call_api(params)


def init_anthropic_compat_client(model_cfg) -> AnthropicCompatLLM:
    """Matches the `init_client` hook's expected signature (LLMModelConfig
    -> LLMInterface) -- see openevolve/llm/claude_code.py's own docstring
    for the same pattern. Assign to LLMModelConfig.init_client, not called
    directly."""
    return AnthropicCompatLLM(model_cfg)
