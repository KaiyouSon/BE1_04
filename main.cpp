#include <Novice.h>
#include <array>
#include <cpprest/filestream.h>
#include <cpprest/http_client.h>
#define _SILENCE_STDEXT_ARR_ITERS_DEPRCATION_WARNING

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace concurrency::streams;

const char kWindowTitle[] = "学籍番号";

enum class GameState {
	None,          // なし
	GameStart,     // ゲーム開始
	Gaming,        // ゲーム中
	RegisterScore, // スコア登録中
	ShowRanking,   // ランキング表示
};

template<class T> pplx::task<T> Get(const std::wstring& url);
pplx::task<int> Post(const std::wstring& url, int score);

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	// ライブラリの初期化
	Novice::Initialize(kWindowTitle, 1280, 720);

	// キー入力結果を受け取る箱
	char keys[256] = {0};
	char preKeys[256] = {0};

	GameState gameState = GameState::GameStart;
	int score = 0;

	const int perFrame = 60;
	const int perSecond = 10;
	int timer = 0;
	int maxTimer = perFrame * perSecond;

	std::wstring url = L"http://localhost:3000/scores/";
	std::array<int, 5> ranking;

	bool isConnectAPI = false;

	// ウィンドウの×ボタンが押されるまでループ
	while (Novice::ProcessMessage() == 0) {
		// フレームの開始
		Novice::BeginFrame();

		// キー入力を受け取る
		memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);

		///
		/// ↓更新処理ここから
		///

		switch (gameState) {
		case GameState::GameStart:

			if (keys[DIK_SPACE] && !preKeys[DIK_SPACE]) {
				gameState = GameState::Gaming;
			}
			break;

		case GameState::Gaming: {

			timer++;
			if (timer >= maxTimer) {
				timer = maxTimer;
			}

			const float timerRete = (float)timer / (float)maxTimer;
			const float rate = 2.5f;
			const float temp = timer * rate * timerRete;
			score += (int)temp;

			if (keys[DIK_SPACE] && !preKeys[DIK_SPACE]) {
				gameState = GameState::RegisterScore;
				isConnectAPI = true;
			}
		} break;

		case GameState::RegisterScore:

			if (isConnectAPI) {
				try {

					// Post通信を行う
					auto serverStatusCode = Post(url, score).get();
					Novice::ScreenPrintf(32, 64, "serverStatusCode = %d", serverStatusCode);

					// スコアの登録が完了したら、ランキングを取得する。POSTが上手く行くと１が返ってくる
					if (serverStatusCode == 200) {

						// 投稿に成功したらGET通信でランキングを取得する
						auto task = Get<json::value>(url);
						const json::value j = task.get();
						auto array = j.as_array();

						// JSONオブジェクトから必要部分を切り出してint型の配列に代入
						for (int i = 0; i < array.size(); i++) {
							ranking[i] = array[i].at(U("score")).as_integer();
						}

						isConnectAPI = false;
					}
				} catch (const std::exception& e) {
					Novice::ConsolePrintf("Error exception:%s\n", e.what()); // 例外処理
				}
			} else {
				gameState = GameState::ShowRanking;
			}

			break;

		case GameState::ShowRanking:

			for (int i = 0; i < ranking.size(); i++) {
				Novice::ScreenPrintf(256, 256 + i * 24, "Ranking[%d] = %d", i, ranking[i]);
			}

			if (keys[DIK_SPACE] && !preKeys[DIK_SPACE]) {
				gameState = GameState::GameStart;
			}

			break;

		default:
			break;
		}

		///
		/// ↑更新処理ここまで
		///

		///
		/// ↓描画処理ここから
		///

		switch (gameState) {
		case GameState::GameStart: {

			Novice::ScreenPrintf(0, 0, "GameState::GameStart");
		} break;

		case GameState::Gaming: {

			Novice::ScreenPrintf(0, 0, "GameState::Gaming");
			Novice::ScreenPrintf(256, 256, "timer = %d", timer);
			Novice::ScreenPrintf(256, 256 + 24, "score = %d", score);
		} break;

		case GameState::RegisterScore: {

			// APIに通信を行う
			Novice::ScreenPrintf(0, 0, "GameState::RegisterScore");
		} break;

		case GameState::ShowRanking: {

			Novice::ScreenPrintf(0, 0, "GameState::ShowRanking");
		} break;

		default:
			break;
		}

		///
		/// ↑描画処理ここまで
		///

		// フレームの終了
		Novice::EndFrame();

		// ESCキーが押されたらループを抜ける
		if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
			break;
		}
	}

	// ライブラリの終了
	Novice::Finalize();
	return 0;
}

template<class T> pplx::task<T> Get(const std::wstring& url) {
	return pplx::create_task([=] {
		       // クライアント定義
		       http_client client(url);

		       // リクエストを送信
		       return client.request(methods::GET);
	       })
	    .then([](http_response response) {
		    // ステータスコード判定
		    if (response.status_code() == status_codes::OK) {
			    // レスポンスボディを表示
			    return response.extract_json();
		    } else {
			    throw std::runtime_error("Received non-OK HTTP status code");
		    }
	    });
}

pplx::task<int> Post(const std::wstring& url, int score) {
	return pplx::create_task([=] {
		       json::value postData;
		       postData[L"score"] = score;

		       http_client client(url);
		       return client.request(methods::POST, L"", postData.serialize(), L"application/json");
	       })
	    .then([](http_response response) {
		    // ステータスコード判定
		    if (response.status_code() == status_codes::OK) {
			    return response.extract_json();
		    } else {
			    throw std::runtime_error("Received non-OK HTTP status code");
		    }
	    })
	    .then([](json::value json) { return json[L"status_code"].as_integer(); });
}
