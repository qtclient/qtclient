#!/usr/bin/env bash
QTCLIENT="$(cd "$(dirname "$0")/../../.." && pwd)"
echo "docs: $QTCLIENT/AGENTS.md $QTCLIENT/include/qtclient/README.md $QTCLIENT/README.md"
