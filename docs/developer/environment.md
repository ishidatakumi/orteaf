# 開発環境ガイド

## Linux / 共通 (Docker)
- Docker を利用して共通のツールチェーンを提供。
- イメージビルド:
  ```bash
  docker build -f docker/dev/Dockerfile -t orteaf-dev .
  ```
- 作業フォルダをマウントしてシェルを起動:
  ```bash
  docker run --rm -it -v "$(pwd)":/workspace -w /workspace orteaf-dev
  ```
- イメージには `clang`, `cmake`, `ninja`, `doxygen`, `graphviz` などが含まれる。
- CUDA を利用する場合は別途 `nvidia/cuda` ベースの派生イメージを作成する予定（未実装）。

## macOS (MPS)
- Metal (MPS) 用の依存関係は Docker で扱えないため、ホスト macOS 上でセットアップする。
- 暫定的に `scripts/setup-mps.sh` を用意。現状はプレースホルダーなので、具体的な手順が固まり次第実装する。
- 少なくとも以下を想定:
  - Xcode Command Line Tools のインストール
  - Homebrew でのツール導入 (`cmake`, `ninja`, `doxygen` 等)
  - 環境変数（`DEVELOPER_DIR` など）の設定

## Windows
- 推奨する方法は WSL2 + Docker。WSL 上で Linux 版コンテナを利用し、同じツールチェーンを共有する。
- ネイティブ Windows 手順は今後必要に応じて追加する。

## Doxygen
- Docker イメージに Doxygen を含めているため、コンテナ内で以下を実行すればドキュメント生成が可能:
  ```bash
  doxygen docs/Doxyfile.user         # 公開 API ドキュメント
  doxygen docs/Doxyfile.developer    # 開発者向けドキュメント
  doxygen docs/Doxyfile.tests        # テストスイート専用ドキュメント
  ```
- 出力先はそれぞれ `docs/api-user/`、`docs/api-developer/`、`docs/api-tests/`。

> このガイドは暫定版です。環境構築手順が固まり次第アップデートしてください。
