SCOPE=src/

.PHONY: format lint test


format:
	uv run ruff format $(SCOPE)
	uv run ruff check --fix --exit-zero $(SCOPE)

lint:
	uv run ruff check $(SCOPE)
	uv run ruff format --check $(SCOPE)

test: 
	uv run python -m unittest discover tests