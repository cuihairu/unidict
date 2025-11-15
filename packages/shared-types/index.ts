// 常用类型定义
export interface BaseResponse<T = any> {
  code: number;
  message: string;
  data: T;
  success: boolean;
  timestamp: number;
}

export interface PaginationRequest {
  page: number;
  pageSize: number;
  sort?: string;
  order?: 'asc' | 'desc';
}

export interface PaginationResponse<T = any> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
  totalPages: number;
}

// 用户相关类型
export interface User {
  id: string;
  username: string;
  email: string;
  nickname: string;
  avatar?: string;
  role: UserRole;
  status: UserStatus;
  createdAt: string;
  updatedAt: string;
  lastLoginAt?: string;
}

export enum UserRole {
  ADMIN = 'admin',
  USER = 'user',
  PREMIUM = 'premium',
  VIP = 'vip'
}

export enum UserStatus {
  ACTIVE = 'active',
  INACTIVE = 'inactive',
  BANNED = 'banned',
  PENDING = 'pending'
}

export interface UserProfile {
  id: string;
  userId: string;
  bio?: string;
  location?: string;
  website?: string;
  birthDate?: string;
  gender?: 'male' | 'female' | 'other';
  language: string;
  timezone: string;
  preferences: UserPreferences;
}

export interface UserPreferences {
  theme: 'light' | 'dark' | 'auto';
  language: string;
  notifications: NotificationSettings;
  privacy: PrivacySettings;
  learning: LearningSettings;
}

export interface NotificationSettings {
  email: boolean;
  push: boolean;
  review: boolean;
  progress: boolean;
  updates: boolean;
}

export interface PrivacySettings {
  showProfile: boolean;
  showProgress: boolean;
  allowDataCollection: boolean;
  shareAnalytics: boolean;
}

export interface LearningSettings {
  dailyGoal: number;
  reviewMode: 'normal' | 'intensive' | 'casual';
  pronunciation: 'us' | 'uk' | 'au';
  autoPlay: boolean;
  showHints: boolean;
}

// 认证相关类型
export interface LoginRequest {
  username: string;
  password: string;
  rememberMe?: boolean;
  captcha?: string;
}

export interface LoginResponse {
  user: User;
  token: string;
  refreshToken: string;
  expiresAt: number;
}

export interface RegisterRequest {
  username: string;
  email: string;
  password: string;
  confirmPassword: string;
  nickname: string;
  captcha?: string;
  inviteCode?: string;
}

// 词典相关类型
export interface Word {
  id: string;
  word: string;
  pronunciation: Pronunciation[];
  definitions: Definition[];
  examples: Example[];
  synonyms: string[];
  antonyms: string[];
  etymology?: string;
  frequency: number;
  tags: string[];
  createdAt: string;
  updatedAt: string;
}

export interface Pronunciation {
  type: 'us' | 'uk' | 'au';
  phonetic: string;
  audioUrl?: string;
}

export interface Definition {
  partOfSpeech: PartOfSpeech;
  meaning: string;
  translation?: string;
  level: LanguageLevel;
  domain?: string;
}

export enum PartOfSpeech {
  NOUN = 'noun',
  VERB = 'verb',
  ADJECTIVE = 'adjective',
  ADVERB = 'adverb',
  PRONOUN = 'pronoun',
  PREPOSITION = 'preposition',
  CONJUNCTION = 'conjunction',
  INTERJECTION = 'interjection',
  ARTICLE = 'article',
  DETERMINER = 'determiner'
}

export enum LanguageLevel {
  BEGINNER = 'beginner',
  ELEMENTARY = 'elementary',
  INTERMEDIATE = 'intermediate',
  UPPER_INTERMEDIATE = 'upper_intermediate',
  ADVANCED = 'advanced',
  PROFICIENT = 'proficient'
}

export interface Example {
  sentence: string;
  translation?: string;
  audioUrl?: string;
  source?: string;
}

export interface SearchRequest {
  query: string;
  type: SearchType;
  filters?: SearchFilters;
  pagination: PaginationRequest;
}

export enum SearchType {
  EXACT = 'exact',
  FUZZY = 'fuzzy',
  PREFIX = 'prefix',
  FULLTEXT = 'fulltext'
}

export interface SearchFilters {
  partOfSpeech?: PartOfSpeech[];
  level?: LanguageLevel[];
  tags?: string[];
  domains?: string[];
}

export interface SearchResponse {
  results: Word[];
  suggestions: string[];
  pagination: PaginationResponse<Word>;
  facets: SearchFacets;
}

export interface SearchFacets {
  partOfSpeech: FacetItem[];
  level: FacetItem[];
  tags: FacetItem[];
  domains: FacetItem[];
}

export interface FacetItem {
  value: string;
  count: number;
}

// 学习相关类型
export interface Vocabulary {
  id: string;
  userId: string;
  name: string;
  description?: string;
  color?: string;
  icon?: string;
  wordCount: number;
  isDefault: boolean;
  isShared: boolean;
  createdAt: string;
  updatedAt: string;
}

export interface VocabularyWord {
  id: string;
  vocabularyId: string;
  wordId: string;
  word: Word;
  note?: string;
  tags: string[];
  mastery: MasteryLevel;
  addedAt: string;
  lastReviewedAt?: string;
  nextReviewAt?: string;
}

export enum MasteryLevel {
  NEW = 'new',
  LEARNING = 'learning',
  FAMILIAR = 'familiar',
  KNOWN = 'known',
  MASTERED = 'mastered'
}

export interface StudySession {
  id: string;
  userId: string;
  vocabularyId?: string;
  type: StudyType;
  duration: number;
  wordsStudied: number;
  correctAnswers: number;
  wrongAnswers: number;
  accuracy: number;
  startedAt: string;
  completedAt: string;
}

export enum StudyType {
  FLASHCARD = 'flashcard',
  LISTENING = 'listening',
  SPELLING = 'spelling',
  PRONUNCIATION = 'pronunciation',
  READING = 'reading',
  WRITING = 'writing'
}

export interface ReviewPlan {
  userId: string;
  totalWords: number;
  todayTarget: number;
  reviewWords: VocabularyWord[];
  newWords: VocabularyWord[];
  scheduledAt: string;
  completedAt?: string;
}

export interface LearningProgress {
  userId: string;
  totalWords: number;
  masteredWords: number;
  learningWords: number;
  newWords: number;
  streakDays: number;
  studyTimeTotal: number;
  studyTimeToday: number;
  accuracy: number;
  level: LanguageLevel;
  experience: number;
  achievements: Achievement[];
  updatedAt: string;
}

export interface Achievement {
  id: string;
  name: string;
  description: string;
  icon: string;
  type: AchievementType;
  requirement: number;
  progress: number;
  isUnlocked: boolean;
  unlockedAt?: string;
}

export enum AchievementType {
  WORDS_LEARNED = 'words_learned',
  STUDY_STREAK = 'study_streak',
  ACCURACY = 'accuracy',
  TIME_SPENT = 'time_spent',
  VOCABULARY_SIZE = 'vocabulary_size'
}

// AI服务类型
export interface TranslationRequest {
  text: string;
  sourceLanguage: string;
  targetLanguage: string;
  style?: TranslationStyle;
  domain?: string;
}

export enum TranslationStyle {
  FORMAL = 'formal',
  INFORMAL = 'informal',
  ACADEMIC = 'academic',
  BUSINESS = 'business',
  CASUAL = 'casual'
}

export interface TranslationResponse {
  originalText: string;
  translatedText: string;
  sourceLanguage: string;
  targetLanguage: string;
  confidence: number;
  alternatives?: string[];
  engine: string;
}

export interface WritingAssistRequest {
  text: string;
  type: WritingAssistType;
  options?: WritingAssistOptions;
}

export enum WritingAssistType {
  GRAMMAR_CHECK = 'grammar_check',
  STYLE_IMPROVE = 'style_improve',
  EXPAND_CONTENT = 'expand_content',
  SUMMARIZE = 'summarize'
}

export interface WritingAssistOptions {
  language: string;
  style: WritingStyle;
  audience: string;
  formality: 'formal' | 'informal';
}

export enum WritingStyle {
  ACADEMIC = 'academic',
  BUSINESS = 'business',
  CREATIVE = 'creative',
  TECHNICAL = 'technical',
  CASUAL = 'casual'
}

export interface WritingAssistResponse {
  originalText: string;
  suggestions: WritingSuggestion[];
  improvedText?: string;
  score: GrammarScore;
}

export interface WritingSuggestion {
  type: SuggestionType;
  position: TextPosition;
  original: string;
  suggestion: string;
  explanation: string;
  confidence: number;
}

export enum SuggestionType {
  GRAMMAR = 'grammar',
  SPELLING = 'spelling',
  PUNCTUATION = 'punctuation',
  STYLE = 'style',
  VOCABULARY = 'vocabulary',
  CLARITY = 'clarity'
}

export interface TextPosition {
  start: number;
  end: number;
  line?: number;
  column?: number;
}

export interface GrammarScore {
  overall: number;
  grammar: number;
  spelling: number;
  style: number;
  clarity: number;
}

// 多媒体类型
export interface AudioResource {
  id: string;
  url: string;
  format: AudioFormat;
  duration: number;
  size: number;
  quality: AudioQuality;
  createdAt: string;
}

export enum AudioFormat {
  MP3 = 'mp3',
  WAV = 'wav',
  OGG = 'ogg',
  AAC = 'aac'
}

export enum AudioQuality {
  LOW = 'low',
  MEDIUM = 'medium',
  HIGH = 'high',
  LOSSLESS = 'lossless'
}

export interface SpeechEvaluationRequest {
  audioData: Blob | File;
  referenceText: string;
  language: string;
  evaluationType: SpeechEvaluationType;
}

export enum SpeechEvaluationType {
  PRONUNCIATION = 'pronunciation',
  FLUENCY = 'fluency',
  ACCURACY = 'accuracy',
  COMPREHENSIVE = 'comprehensive'
}

export interface SpeechEvaluationResponse {
  overallScore: number;
  pronunciationScore: number;
  fluencyScore: number;
  accuracyScore: number;
  feedback: SpeechFeedback[];
  audioAnalysis: AudioAnalysis;
}

export interface SpeechFeedback {
  type: FeedbackType;
  position: AudioPosition;
  word: string;
  score: number;
  suggestion: string;
  severity: 'low' | 'medium' | 'high';
}

export enum FeedbackType {
  MISPRONUNCIATION = 'mispronunciation',
  STRESS_ERROR = 'stress_error',
  RHYTHM_ERROR = 'rhythm_error',
  INTONATION_ERROR = 'intonation_error'
}

export interface AudioPosition {
  startTime: number;
  endTime: number;
}

export interface AudioAnalysis {
  duration: number;
  pitch: PitchAnalysis;
  rhythm: RhythmAnalysis;
  volume: VolumeAnalysis;
}

export interface PitchAnalysis {
  average: number;
  range: number;
  variance: number;
}

export interface RhythmAnalysis {
  pauseCount: number;
  speechRate: number;
  regularity: number;
}

export interface VolumeAnalysis {
  average: number;
  peak: number;
  consistency: number;
}

// OCR类型
export interface OCRRequest {
  image: Blob | File;
  language: string;
  options?: OCROptions;
}

export interface OCROptions {
  detectOrientation: boolean;
  preserveLayout: boolean;
  outputFormat: 'text' | 'json' | 'xml';
}

export interface OCRResponse {
  text: string;
  confidence: number;
  words: OCRWord[];
  layout: LayoutInfo;
  language: string;
}

export interface OCRWord {
  text: string;
  confidence: number;
  bbox: BoundingBox;
}

export interface BoundingBox {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface LayoutInfo {
  orientation: number;
  textAngle: number;
  lines: LineInfo[];
}

export interface LineInfo {
  text: string;
  bbox: BoundingBox;
  words: OCRWord[];
}

// 系统类型
export interface SystemInfo {
  version: string;
  buildTime: string;
  environment: string;
  features: string[];
}

export interface HealthCheck {
  status: 'healthy' | 'degraded' | 'unhealthy';
  timestamp: string;
  services: ServiceHealth[];
  uptime: number;
}

export interface ServiceHealth {
  name: string;
  status: 'up' | 'down' | 'degraded';
  responseTime: number;
  lastChecked: string;
  error?: string;
}

export interface APIError {
  code: string;
  message: string;
  details?: Record<string, any>;
  timestamp: string;
  requestId?: string;
}

// API端点类型
export interface APIEndpoint {
  method: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH';
  path: string;
  description: string;
  parameters?: Parameter[];
  requestBody?: any;
  responses: Record<string, any>;
}

export interface Parameter {
  name: string;
  type: string;
  required: boolean;
  description: string;
  example?: any;
}