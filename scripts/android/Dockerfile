# Build environment for the Android APK: JDK 21, the Android SDK and NDK,
# and Qt for Android with the matching desktop Qt (for moc, rcc and
# androiddeployqt). Versions follow the Qt 6.11 Android requirements.
FROM eclipse-temurin:21-jdk-noble

ARG QT_VERSION=6.11.3
ARG NDK_VERSION=27.2.12479018
ARG ANDROID_PLATFORM=android-36
ARG BUILD_TOOLS=36.0.0
ARG CMDLINE_TOOLS=11076708

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake ninja-build python3 python3-venv unzip curl ca-certificates \
        libgl1 libglib2.0-0 libfontconfig1 libxkbcommon0 \
    && rm -rf /var/lib/apt/lists/*

ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV ANDROID_HOME=/opt/android-sdk
RUN mkdir -p $ANDROID_SDK_ROOT/cmdline-tools \
    && curl -fsSL -o /tmp/tools.zip \
        https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS}_latest.zip \
    && unzip -q /tmp/tools.zip -d $ANDROID_SDK_ROOT/cmdline-tools \
    && mv $ANDROID_SDK_ROOT/cmdline-tools/cmdline-tools $ANDROID_SDK_ROOT/cmdline-tools/latest \
    && rm /tmp/tools.zip
ENV PATH=$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$ANDROID_SDK_ROOT/platform-tools:$PATH
RUN yes | sdkmanager --licenses > /dev/null \
    && sdkmanager --install "platform-tools" "platforms;${ANDROID_PLATFORM}" \
        "build-tools;${BUILD_TOOLS}" "ndk;${NDK_VERSION}" > /dev/null
ENV ANDROID_NDK_ROOT=$ANDROID_SDK_ROOT/ndk/${NDK_VERSION}

RUN python3 -m venv /opt/aqt && /opt/aqt/bin/pip install --quiet aqtinstall \
    && /opt/aqt/bin/aqt install-qt all_os android ${QT_VERSION} android_arm64_v8a \
        --autodesktop -O /opt/Qt \
    && rm -rf /opt/aqt aqtinstall.log
ENV QT_ANDROID=/opt/Qt/${QT_VERSION}/android_arm64_v8a
ENV QT_HOST=/opt/Qt/${QT_VERSION}/gcc_64

# Gradle downloads its distribution and the Android Gradle plugin on first
# use; build.sh keeps them in a volume so later builds skip the download.
ENV GRADLE_USER_HOME=/cache/gradle
WORKDIR /src
