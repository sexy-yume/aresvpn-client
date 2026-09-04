<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<!--
  AresVPN Client - the Korean for the strings THIS FORK added.

  AresProject ROADMAP 18-3h. `client/translations/` is upstream's and is never written (18-3b,
  #D180 rule 3); `derive-translations.py` reads it and produces the fork's .ts files. That
  mechanism rewrites Amnezia's marks inside strings that ALREADY EXIST. It cannot invent an entry
  for a string upstream never had - and the screens this phase rebuilt are full of those.

  MEASURED, not assumed: of 93 unique `qsTr` strings across the eight screens we own, **22 were
  already Korean and 71 were not**, which is why a render on the real Windows platform showed
  "설정" and "연결" in Korean beside "Rents" and "Add a rent" in English on the same screen. This
  product's first market is Korea (`AresStyle.qml` says so where the Hangul fallback is
  registered), so half a translated UI is a product defect rather than a nicety.

  WHAT IS DELIBERATELY LEFT IN ENGLISH, and each for a reason rather than by omission:
    * `AresVPN Client`            - the product mark. A mark is not translated (#D178).
    * `github.com/...`, `ares-vpn.org`, `ares-vpn.org/support`, `support@ares-vpn.org`
                                  - URLs and an address. `derive-translations.py` refuses to
                                    rewrite inside one for the same reason (#L007: an operator
                                    input, not something a tool invents).
    * `%1 d`                      - the compact days-left chip; the unit stays a single letter so
                                    the chip keeps its width on every locale.

  TERMINOLOGY IS THE CONSOLE'S, not this file's invention. `server/src/i18n.ts` says **렌트** for a
  rent throughout ("nodes.rents": "렌트", "ip.rentCount": "렌트 {0}건"), and a customer who reads
  "렌트" on their reseller's screen must read the same word in the client. CLAUDE.md section 0's
  rule - write "rent", never "key" - is a terminology rule in both languages.

  The context name is the QML file's base name, which is what `qsTr` in a .qml file uses.
-->
<TS version="2.1" language="ko_KR">
<context>
    <name>PageHome</name>
    <message>
        <source>No rent yet</source>
        <translation>아직 렌트가 없습니다</translation>
    </message>
    <message>
        <source>Add one with your account id, password and its idx</source>
        <translation>계정 아이디와 비밀번호, 그리고 렌트의 idx로 추가하세요</translation>
    </message>
    <message>
        <source>Rents</source>
        <translation>렌트</translation>
    </message>
    <message>
        <source>Switch, add or remove</source>
        <translation>전환, 추가, 삭제</translation>
    </message>
</context>
<context>
    <name>PageAresRents</name>
    <message>
        <source>Rents</source>
        <translation>렌트</translation>
    </message>
    <message>
        <source>Each rent is one public address on one node. Tap to make it the one you connect with.</source>
        <translation>렌트 하나는 노드 한 대의 공인 IP 하나입니다. 눌러서 접속에 사용할 렌트로 지정하세요.</translation>
    </message>
    <message>
        <source>Disconnect before switching to another rent.</source>
        <translation>다른 렌트로 바꾸려면 먼저 연결을 끊으세요.</translation>
    </message>
    <message>
        <source>EXPIRED</source>
        <translation>만료됨</translation>
    </message>
    <message>
        <source>LIVE</source>
        <translation>연결됨</translation>
    </message>
    <message>
        <source>SELECTED</source>
        <translation>선택됨</translation>
    </message>
    <message>
        <source>Disconnect before removing the rent you are using.</source>
        <translation>사용 중인 렌트를 삭제하려면 먼저 연결을 끊으세요.</translation>
    </message>
    <message>
        <source>An HTTP proxy rent will not appear here - it is used from a browser or curl, with the address on its page in the AresVPN console.</source>
        <translation>HTTP 프록시 렌트는 여기에 나오지 않습니다. 브라우저나 curl에서 AresVPN 콘솔의 렌트 페이지에 적힌 주소로 사용합니다.</translation>
    </message>
    <message>
        <source>Add a rent</source>
        <translation>렌트 추가</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>%1을(를) 삭제할까요?</translation>
    </message>
    <message>
        <source>The rent itself is not cancelled - only this device forgets it. Log in with the same idx to get it back.</source>
        <translation>렌트 자체가 해지되는 것은 아니고, 이 기기에서만 잊힙니다. 같은 idx로 다시 로그인하면 그대로 돌아옵니다.</translation>
    </message>
    <message>
        <source>Keep</source>
        <translation>유지</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>삭제</translation>
    </message>
</context>
<context>
    <name>PageSettings</name>
    <message>
        <source>Rents</source>
        <translation>렌트</translation>
    </message>
    <message>
        <source>About</source>
        <translation>정보</translation>
    </message>
    <message>
        <source>Add, switch or remove a rent</source>
        <translation>렌트 추가, 전환, 삭제</translation>
    </message>
    <message>
        <source>Kill switch, split tunnelling and DNS</source>
        <translation>킬 스위치, 스플릿 터널링, DNS</translation>
    </message>
    <message>
        <source>Language, autostart, notifications and logging</source>
        <translation>언어, 자동 시작, 알림, 로그</translation>
    </message>
</context>
<context>
    <name>PageSettingsConnection</name>
    <message>
        <source>What the tunnel does to this machine.</source>
        <translation>터널이 이 기기에 적용하는 설정입니다.</translation>
    </message>
    <message>
        <source>Which resolver this device uses while connected</source>
        <translation>연결 중에 이 기기가 사용할 DNS 서버</translation>
    </message>
    <message>
        <source>Split tunnelling by site</source>
        <translation>사이트별 스플릿 터널링</translation>
    </message>
    <message>
        <source>Choose which sites go through the rent</source>
        <translation>어떤 사이트를 렌트로 보낼지 선택합니다</translation>
    </message>
    <message>
        <source>Split tunnelling by app</source>
        <translation>앱별 스플릿 터널링</translation>
    </message>
    <message>
        <source>Choose which applications go through the rent</source>
        <translation>어떤 앱을 렌트로 보낼지 선택합니다</translation>
    </message>
    <message>
        <source>Kill switch</source>
        <translation>킬 스위치</translation>
    </message>
    <message>
        <source>Cut this device off the network if the tunnel drops</source>
        <translation>터널이 끊기면 이 기기의 네트워크를 차단합니다</translation>
    </message>
</context>
<context>
    <name>PageSettingsApplication</name>
    <message>
        <source>What the app does to itself.</source>
        <translation>앱 자체의 동작 설정입니다.</translation>
    </message>
    <message>
        <source>Show the tunnel&apos;s state in the status bar</source>
        <translation>상태 표시줄에 터널 상태를 표시합니다</translation>
    </message>
    <message>
        <source>Allow screenshots</source>
        <translation>스크린샷 허용</translation>
    </message>
    <message>
        <source>Start with the system</source>
        <translation>시스템 시작 시 실행</translation>
    </message>
    <message>
        <source>Launch AresVPN when this device starts</source>
        <translation>이 기기가 켜질 때 AresVPN을 실행합니다</translation>
    </message>
    <message>
        <source>Connect on launch</source>
        <translation>실행 시 자동 연결</translation>
    </message>
    <message>
        <source>Bring the rent up as soon as the app opens</source>
        <translation>앱이 열리는 즉시 렌트에 연결합니다</translation>
    </message>
    <message>
        <source>Start minimised</source>
        <translation>최소화 상태로 시작</translation>
    </message>
    <message>
        <source>Only applies when starting with the system</source>
        <translation>시스템 시작 시 실행에만 적용됩니다</translation>
    </message>
    <message>
        <source>Reset and remove all data</source>
        <translation>초기화 및 모든 데이터 삭제</translation>
    </message>
    <message>
        <source>Every rent on this device is forgotten</source>
        <translation>이 기기의 모든 렌트가 잊힙니다</translation>
    </message>
    <message>
        <source>All settings will be reset to default and every rent this device holds will be forgotten. The rents themselves are not cancelled - log in again with the same idx to get them back.</source>
        <translation>모든 설정이 기본값으로 돌아가고, 이 기기가 가진 모든 렌트가 잊힙니다. 렌트 자체가 해지되는 것은 아니며, 같은 idx로 다시 로그인하면 그대로 돌아옵니다.</translation>
    </message>
</context>
<context>
    <name>PageSettingsAbout</name>
    <message>
        <source>Copyright (c) 2026 AresVPN. A fork of Amnezia VPN, copyright (c) the AmneziaVPN authors. Amnezia and AmneziaVPN are their marks and this product is not affiliated with them.

This program comes with ABSOLUTELY NO WARRANTY.

This is free software, and you are welcome to redistribute it and to change it under the terms of the GNU General Public License, version 3.</source>
        <translation>Copyright (c) 2026 AresVPN. Amnezia VPN의 포크이며, 원저작권은 AmneziaVPN 개발자들에게 있습니다. Amnezia와 AmneziaVPN은 그들의 상표이고, 본 제품은 그들과 아무런 제휴 관계가 없습니다.

이 프로그램에는 어떠한 보증도 없습니다.

이 프로그램은 자유 소프트웨어이며, GNU General Public License 버전 3의 조건에 따라 자유롭게 재배포하고 수정할 수 있습니다.</translation>
    </message>
    <message>
        <source>Licences</source>
        <translation>라이선스</translation>
    </message>
    <message>
        <source>The GPL-3, and the terms of everything this program includes</source>
        <translation>GPL-3, 그리고 이 프로그램이 포함하는 모든 구성 요소의 이용 조건</translation>
    </message>
    <message>
        <source>Source code</source>
        <translation>소스 코드</translation>
    </message>
    <message>
        <source>Contact</source>
        <translation>문의</translation>
    </message>
    <message>
        <source>Support</source>
        <translation>고객지원</translation>
    </message>
</context>
<context>
    <name>PageSettingsLicenses</name>
    <message>
        <source>Licences</source>
        <translation>라이선스</translation>
    </message>
    <message>
        <source>GNU General Public License v3</source>
        <translation>GNU General Public License v3</translation>
    </message>
    <message>
        <source>The licence this program is released under</source>
        <translation>이 프로그램이 배포되는 라이선스</translation>
    </message>
    <message>
        <source>Third-party components</source>
        <translation>제3자 구성 요소</translation>
    </message>
    <message>
        <source>What this program includes, and under what terms</source>
        <translation>이 프로그램이 포함하는 것과 그 이용 조건</translation>
    </message>
    <message>
        <source>SIL Open Font License - PT Root UI</source>
        <translation>SIL Open Font License - PT Root UI</translation>
    </message>
    <message>
        <source>The licence of the typeface this interface is set in</source>
        <translation>이 화면에 사용된 글꼴의 라이선스</translation>
    </message>
    <message>
        <source>Wintun prebuilt binaries licence</source>
        <translation>Wintun 사전 빌드 바이너리 라이선스</translation>
    </message>
    <message>
        <source>The terms of the Windows tunnel driver this product ships</source>
        <translation>본 제품이 함께 배포하는 Windows 터널 드라이버의 이용 조건</translation>
    </message>
    <message>
        <source>AresVPN Client is free software. You may redistribute it and change it under the terms of the GNU General Public License, version 3. It comes with ABSOLUTELY NO WARRANTY.</source>
        <translation>AresVPN Client는 자유 소프트웨어입니다. GNU General Public License 버전 3의 조건에 따라 재배포하고 수정할 수 있습니다. 어떠한 보증도 제공되지 않습니다.</translation>
    </message>
    <message>
        <source>This licence text could not be read from the application (%1).

The same texts are installed beside the program&apos;s executable, and the complete source of this program is at https://github.com/sexy-yume/aresvpn-client</source>
        <translation>이 라이선스 본문을 앱에서 읽지 못했습니다 (%1).

같은 문서가 프로그램 실행 파일과 같은 위치에 설치되어 있으며, 이 프로그램의 전체 소스는 https://github.com/sexy-yume/aresvpn-client 에 있습니다</translation>
    </message>
</context>
<context>
    <name>PageSetupWizardAresLogin</name>
    <message>
        <source>AresVPN account</source>
        <translation>AresVPN 계정</translation>
    </message>
    <message>
        <source>Your account id and password, and the idx of the rent you want on this device. The idx is the alias you or your reseller gave the rent in the AresVPN console.</source>
        <translation>계정 아이디와 비밀번호, 그리고 이 기기에서 사용할 렌트의 idx를 입력하세요. idx는 AresVPN 콘솔에서 본인 또는 총판이 그 렌트에 붙인 별칭입니다.</translation>
    </message>
    <message>
        <source>Account id</source>
        <translation>계정 아이디</translation>
    </message>
    <message>
        <source>your login</source>
        <translation>로그인 아이디</translation>
    </message>
    <message>
        <source>Rent idx</source>
        <translation>렌트 idx</translation>
    </message>
    <message>
        <source>for example odin_1</source>
        <translation>예: odin_1</translation>
    </message>
    <message>
        <source>Add this rent</source>
        <translation>이 렌트 추가</translation>
    </message>
    <message>
        <source>Your password is sent once, over TLS, to %1 and is not stored on this device. The rent&apos;s configuration is.</source>
        <translation>비밀번호는 TLS로 %1에 한 번만 전송되며 이 기기에 저장되지 않습니다. 저장되는 것은 렌트의 설정 파일입니다.</translation>
    </message>
    <message>
        <source>All three fields are needed.</source>
        <translation>세 항목을 모두 입력해야 합니다.</translation>
    </message>
    <message>
        <source>Rent %1 added</source>
        <translation>렌트 %1을(를) 추가했습니다</translation>
    </message>
</context>
</TS>
