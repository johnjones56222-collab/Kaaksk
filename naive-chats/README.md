# Naive Chats

A texting game where every contact is played by an LLM through Naive
(usenaive.ai). Open `index.html` in a browser, paste your Naive API key in
Settings, pick a model, and start texting.

Default contacts, each with their own opening text:

- **Boss**: buzzword-heavy middle manager. Opens with "Bro we lost 283 Robux
  in Revune" (the typo is intended).
- **Cringe Bro**: your actual older brother. Out-of-date slang, a new
  get-rich-quick scheme every week, and Mom wants you to call her.
- **Claude-Obsessed Friend**: brings every topic back to Claude.
- **John Johnson (Accounting)**: the most generic person in history.

Use ＋ to add your own contacts (name, emoji, personality, first text); ✎ edits
them. ↺ starts a chat over. A reply can come back as several texts: the
model separates them with blank lines.

The key, chats, and custom contacts are saved in this browser's localStorage.
Browsers can't call `api.vetta.sh` (Naive's API) directly, so messages go
through the deployed `vetta-video-studio` Worker (`/api/chat` and
`/api/llm-models`), which forwards them with the key the page sends in
`x-vetta-key`.
